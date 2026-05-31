/**
 * @file lsp_dispatcher.hpp   
 * @author Julien FAUCHER
 * @brief This file describes the object that models a "scope" in the index
 * @copyright
 * MIT License
 * 
 * Copyright (c) 2025 Julien FAUCHER
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * SPDX-License-Identifier: MIT
 */


#pragma once


#include <optional>
#include <thread>
#include <stop_token>
#include <future>
#include <mutex>

#include <condition_variable>
#include <random>
#include <queue>
#include <functional>
#include <unordered_map>
#include <set>
#include <iostream>

#include "spdlog/logger.h"
#include "uuid.h"
#include "nlohmann/json.hpp"
#include "rpc_transport.hpp"

#include "lsp_errors.hpp"

using json = nlohmann::json;

/**
 * @brief Helper macro to help binding a member function in diplomat::lsp::LSPCommandDispatcher binds commands.
 * 
 * @sa diplomat::lsp::LSPCommandDispatcher::bind_notification
 * @sa diplomat::lsp::LSPCommandDispatcher::bind_request
 *
 */
#define LSP_MEMBER_BIND(base_cls, fct) std::bind(&base_cls::fct, this, std::placeholders::_1, std::placeholders::_2)

namespace diplomat::lsp {

	/** 
	* A request is a function, taking a json object as its parameter and a stop token in order to 
	* react to cancellation request (if relevant).
	* Such function shall return a JSON object as a value (depending on the actual request).
	*/
	typedef std::function<json(const json&, std::stop_token)> request_handle_t;
	/**
	 * A notification is a function that takes JSON as an argument but is not expected to return any value.
	 * It also includes a stop token to react to cancellation.
	 */
    typedef std::function<void(const json&, std::stop_token)> notification_handle_t;

	MAKE_BASIC_SRV_EXCEPTION(client_timeout_error);

	/**
	 * @brief Exception produced when a cancel request is produced for easy exit of current processing.
	 */
	MAKE_BASIC_SRV_EXCEPTION(client_cancel_request_exception);

	/**
	 * @brief This class implements a generic command dispatcher
	 * 
	 * It is expected that the server only handle one client command at a time.
	 * However, it is very much possible to request multiple requests before completion of an expensive operation.
	 * Therefore, it is mandatory to handle multiples incoming requests.
	 *
	 * Also, the dispatcher shall be able to handle the cancellation requests and reverse requests (from the server to the client).
	 *
	 * Binding methods is done through the #bind_request() and #bind_notification() member functions, associating a string (the command name
	 * to be used on client side) to a function on server side. 
	 * There are two functions formats to be used: 
	 * - For notifications (no return values), the function shall have a prototype such as: <tt>void foo(const json& args, std::stop_token tk);</tt>
	 * - For requests (with return values), the function shall have a prototype such as: <tt>json foo(const json& args, std::stop_token tk);</tt>
	 *
	 * Moreover, if the function may run for a long time, it shall use the std::stop_token argument to detect a used stop request and 
	 * throw a diplomat::lsp::client_cancel_request_exception as soon as possible.
	 *
	 *
	 */
	class LSPCommandDispatcher {
		protected:
			/**
			 * @brief The running flag of the dispatcher.
			 *
			 * It is set to true by the constructor and cleared by the #stop() command.
			 * Setting it to false will allow exiting the #run() function.
			 * In this case, the dispatcher should be discarded.
			 */
			bool _running; 
			/** The attached command interface */
			rpc::RPCPipeTransport _rpc;

			/** Incoming FIFO, storing messages that will be processed later while 
			* waiting for a reeverse request result. */
			std::queue<json> _inbox;

			/** Holds the list of cancelled calls IDs those IDs shall be aborted (if ongoing) or skipped if in #_inbox */
			std::set<std::string> _cancelled_calls;

			/** Random engine used for UUID generation */
			std::mt19937 _rand_engine;
			/** UUID generator */
        	uuids::uuid_random_generator _uuid;

			
			/** Stored handlers for requests (functions with return values) */
			std::unordered_map<std::string, request_handle_t> _bound_requests;
			/** Stored handlers for notifications (functions without return values) */
			std::unordered_map<std::string, notification_handle_t> _bound_notifs;
			/** 
			* Function that is always executed *before* the actual handling of any command
            *
			* Returning false will silently discard the call, while throwing a diplomat::lsp::rpc_base_exception or derived will
			* propagate it appropriately.
			* If unbound, assume that there is no filtering. 
			*/
			std::optional<std::function< bool(const std::string&, const json&)>> _bound_filter;
			
			/** 
			* When receiving non-standards commands (through workspace/executeCommand) the arguments will be 
			* transmitted as an array of objects.
			* If true, unpack this array if only one argument is found.
			* Otherwise, return an empty JSON (if empty array) or the array itself.
			*/
			bool _unpack_non_standard_args;


			/** Main worker */
			std::jthread _worker;
			/** Worker running wrapper */
			std::mutex _work_mutex;
			volatile bool _worker_running;
			volatile bool _worker_done;
			std::condition_variable _worker_cv;
			/** Ongoing ID used for cancellation*/
			json _ongoing_id;
			std::string _ongoing_method;
			nlohmann::json _ongoing_params;

			/** ID used for the last reverse request if any */
			std::optional<std::string> _awaited_id;
			/** Used to wake up the worker if it is waiting on a client data */
			std::condition_variable _client_cb_data_available;
			/** Returned data from a reverse request */
			std::promise<json> _cb_data;
			/** Client timeout in milliseconds */
			unsigned int _client_timeout;

			/** Request the end of the worker, block until then and return */
			void _finish_worker();

			/**
			 * @brief Wait for the worker to start and for the working status flag to be set
			 */
			void _wait_for_worker_start();

			/**
			 * @brief Base function to send an RPC call to the client.
			 * 
			 * The ID field is automatically filled with a random UUID.
			 * @param method method name to invoke
			 * @param params parameter object to send.
			 * @return std::string the generated ID linked to the sent call.
			 */
			std::string _send_rpc_call(const std::string& method, const json& params);

			/**
			 * @brief Base function to send an RPC call to the client.
			 * 
			 * @param method method name to invoke
			 * @param params parameter object to send.
			 * @param id id for the transaction, see the LSP specification for more details.
			 */
			void _send_rpc_call(const std::string& method, const json& params, const std::optional<std::string> id);

			/**
			 * @brief Invoke a registered method, regardless of the fact that it is a notification
			 * or a request.
			 *
			 * This is the main handler function.
			 * 
			 * @param method method name to invoke
			 * @param params JSON object containing the parameters for the method.
			 */
			void _invoke();

			/**
			 * @brief Handler for incoming json messages
			 * 
			 * @param _data JSON message to handle
			 */
			bool _process_input_message(const json& data ); 

			/**
			 * @brief Read the input, act on it for specific messages (replies and cancellations) 
			 * and store the actual calls.
			 * 
			 */
			 void _handle_next_incoming_message();

			 std::shared_ptr<spdlog::logger> _log;


		public:

			/**
			 * @brief Construct a new LSPCommandDispatcher connected to the client through 
			 * the provided streams.
			 *
			 * Recommended usage is: 
			 * \code {.cpp}
			 *	// some code ... 
			 *	{
			 *		LSPCommandDispatcher dispatcher(in, out); 
			 *		dispatcher.run(); 
			 *		// run() is a blocking call, upon exiting, the whole dispatcher should be deleted.
			 *	}
			 *	// some other code, cleanup and stuff.
			 * \endcode
			 * 
			 * @param is Input stream (from client to server)
			 * @param os output stream (from server to client)
			 */
			explicit LSPCommandDispatcher(std::istream& is = std::cin, std::ostream& os = std::cout);
			
			~LSPCommandDispatcher();

			/**
			 * @brief Adds a new *request* handler
			 *
			 * This function allows binding a function that will return a value (an LSP request) to its command name.
			 * example usage, using #LSP_MEMBER_BIND() follows: 
			 * \code{.cpp}
			 *	class MyLSP : public LSPCommandDispatcher
			 *	{
			 *		nlohmann::json _h_rename(nlohmann::json params, std::stop_token tk);
			 *		void binds();
			 *	};
			 *
			 *	void MyLSP::binds()
			 *	{
			 *		bind_request("textDocument/rename", LSP_MEMBER_BIND(MyLSP, _h_rename));
			 *		// Equivalent to:
			 *		bind_request("textDocument/rename", std::bind(&MyLSP::_h_rename, this, std::placeholders::_1, std::placeholders::_2));
			 *	}
			 * \endcode
			 * 
			 * Once bound, the client will be able to invoke the function \p cb through a call to the method using the name \p fct_name.
			 * 
			 * @warning It is essential to notify the client of the availability of those bound methods through the <tt>\ref diplomat::lsp::types::ServerCapabilities::executeCommandProvider "executeCommandProvider"</tt> attribute
			 * of diplomat::lsp::types::ServerCapabilities sent in return of the 
			 * <a href="https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize"><tt>initialize</tt></a> 
			 * request of the LSP.
			 * For non-standard methods, ::transfer_supported_nonstandard_methods() should be used to get all additional methods names 
			 * after binding everything.
			 *
			 * @param fct_name Method name
			 * @param cb Actual handler
			 * @param allow_override Allow replacing an already bound handler.
			 *
			 * @sa ::bind_notification()
			 * @sa ::transfer_supported_nonstandard_methods()
			 * @sa #LSP_MEMBER_BIND()
			 */
			void bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override = false);
        	
			/**
			 * @brief Adds a new *notification* handler
			 *
			 * This function allows binding a function that will not return any value (an LSP notification) to its command name.
			 * example usage, using #LSP_MEMBER_BIND() follows: 
			 * \code{.cpp}
			 *	class MyLSP : public LSPCommandDispatcher
			 *	{
			 *		void _h_initialized(nlohmann::json params, std::stop_token tk);
			 *		void binds();
			 *	};
			 *
			 *	void MyLSP::binds()
			 *	{
			 *		bind_notification("initialized", LSP_MEMBER_BIND(MyLSP,_h_initialized));
			 *		// Equivalent to:
			 *		bind_notification("initialized", std::bind(&MyLSP::_h_initialized, this, std::placeholders::_1, std::placeholders::_2));
			 *	}
			 * \endcode
			 * 
			 * Once bound, the client will be able to invoke the function \p cb through a call to the method using the name \p fct_name.
			 * 
			 * @warning It is essential to notify the client of the availability of those bound methods through the <tt>\ref diplomat::lsp::types::ServerCapabilities::executeCommandProvider "executeCommandProvider"</tt> attribute
			 * of diplomat::lsp::types::ServerCapabilities sent in return of the 
			 * <a href="https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initialize"><tt>initialize</tt></a> 
			 * request of the LSP.
			 * For non-standard methods, ::transfer_supported_nonstandard_methods() should be used to get all additional methods names 
			 * after binding everything.
			 *
			 * @param fct_name Method name
			 * @param cb Actual handler
			 * @param allow_override Allow replacing an already bound handler.
			 *
			 * @sa ::bind_request()
			 * @sa ::transfer_supported_nonstandard_methods()
			 * @sa #LSP_MEMBER_BIND()
			 */
			void bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override = false);
			
			/**
			 * @brief Bind a filtering function in order to allow rejecting all calls before their execution.
			 * 
			 * @sa #_bound_filter
			 * @param filter Filtering function, returning false in order to silently ignore a call or 
			 * throwing an exception for a non-silent rejection.
			 */
			void bind_call_filter(std::function< bool(const std::string&, const json&)> filter);

			/**
			 * @brief Sends a notification to the client
			 * 
			 * @param method Method name to invoke
			 * @param args Arguments object to provide
			 */
			void notify_client(const std::string& method, const json& args);

			/**
			 * @brief Send a request to the client, expecting a return value.
			 *
			 * @warning This function will block the current thread (expected to be the worker thread)
			 * 
			 * @param method Method to invoke
			 * @param args Arguments object to send

			 * @return json Return value
			 * @throws client_cancel_request_exception when interrupted by client request.
			 * @throws client_timeout_error when upon reaching timeout
			 */
			json request_client(const std::string& method, const json& args);

			/**
			 * @brief Send a request to the client, expecting a return value.
			 *
			 * This function returns the future to the returned value to be handled by the caller.
			 * 
			 * @note For the blocking version of this command, see #request_client()
			 * 
			 * @param method Method to invoke
			 * @param args Arguments object to send
			 * @return std::future<json> Return value future object
			 */
			std::future<json> request_client_future(const std::string& method, const json& args);


			/**
			 * @brief Forward exception (messages and such) to the client
			 *
			 * This is actually the principal handler for all RPC related exceptions.
			 * Processes should raise such exception for conversion in an error message
			 * which would then be forwarded.
			 * 
			 * @note This gunction only handle the transmission of the error without underlying
			 * action (no cleanup, no  action on the worker).
			 *
			 * @param e the exception to forward as an error message.
			 */
			void forward_exception(const rpc_base_exception& e);

			/**
			 * @brief Forward result to the client
			 * 
			 * Sends a result message to the client with #val as data field
			 *
			 * @param val  Data to send back
			 */
			void forward_result(const nlohmann::json& val); 
			
			/**
			 * @brief Set the additional endl inserted at the end of RPC messages.
			 * Used for debug and/or compatibility.
			 * 
			 * @param use_endl : new value.
			 */
			inline void set_rpc_use_endl(const bool use_endl){_rpc.set_endl(use_endl);};

			/**
			 * @brief Set the unpack non-standards params property
			 *
			 * When set, the parameters for non-standards calls will be unpacked if they were in an array
			 * 
			 * @param unpack set to true to enable the unpacking
			 */
			inline void set_unpack_nonstandards_params(const bool unpack){_unpack_non_standard_args = unpack;};

			/**
			 * @brief Push the name of all non-LSP standard method bound to this dispatcher.
			 * 
			 * \code {.cpp}
			 * std::vector<std::string> my_list;
			 * dispatcher.transfer_supported_nonstandard_methods(my_list);
			 * \endcode
			 * 
			 *
			 * @param tgt reference to the vector to fill.
			 */
			void transfer_supported_nonstandard_methods(std::vector<std::string>& tgt) const;

			/**
			 * @brief Check if the provided name is a bound notification
			 * 
			 * @param fct method name to check
			 * @return true if \p fct id a bound notification
			 * @return false otherwise
			 */
			bool is_notif(const std::string& fct) const;

			/**
			 * @brief Check if the provided name is a bound request
			 * 
			 * @param fct method name to check
			 * @return true if \p fct id a bound request
			 * @return false otherwise
			 */
			bool is_request(const std::string& fct) const;

			/**
			 * @brief Check if the provided name is a bound methoc
			 * 
			 * @param fct method name to check
			 * @return true if \p fct id a bound method
			 * @return false otherwise
			 */
			bool is_bound(const std::string& fct) const;

			/**
			 * @brief Checks if a given method name refers to a bound non-standard method.
			 *
			 * \sa ::slsp::types::RESERVED_METHODS for the list of reserved method names.
			 * 
			 * @param fct method name to lookup
			 * @return true If the method is not a LSP standard method and matches a bound metchod.
			 * @return false if the method is either a standard LSP method or unbound.
			 */
			bool is_non_standard_method(const std::string& fct) const;

			/**
			 * @brief Runner function. 
			 * 
			 * This is the entry point for the dispatcher.
			 * Once started, this function should not return until the dispatcher can be disposed of.
			 * Meaning that the server is shutting down.
			 */
			void run();

			/**
			 * @brief Request an exit from the dispatcher running function, meaning putting it out of service.
			 * @sa #_running
			 */
			inline void stop() {_running = false;};

			/**
			 * @brief Generate an UUID through the internal UUID generator
			 * 
			 * @note this is made public as a convenience to avoid rebuilding the UUID infrastructure
			 * @return std::string generated UUID
			 */
			inline std::string get_uuid() { return uuids::to_string(_uuid());}
	};
}
