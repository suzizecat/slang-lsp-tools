#include "lsp_dispatcher.hpp"
#include "lsp_errors.hpp"
#include "types/methods/lsp_reserved_methods.hpp"
#include "types/structs/ExecuteCommandParams.hpp"
#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>



namespace diplomat::lsp {


	LSPCommandDispatcher::LSPCommandDispatcher(std::istream& is, std::ostream& os) : 
    _running(true),
	_rpc(is,os),
	_inbox(),
	_cancelled_calls(),
    _uuid(&_rand_engine),
    _bound_requests(),
    _bound_notifs(),
	_unpack_non_standard_args(false),
	_worker(),
	_work_mutex(),
	_worker_running(false),
	_worker_cv(),
	_ongoing_id(nullptr),
	_client_cb_data_available(),
	_cb_data(),
	_client_timeout(0)
    {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size> {};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        _rand_engine = std::mt19937(seq);
    }

	LSPCommandDispatcher::~LSPCommandDispatcher()
	{
		_finish_worker();
	}

	void LSPCommandDispatcher::run()
	{
		using namespace std::chrono_literals;
		spdlog::info("Booting the dispatcher");
		bool started;
		// TODO Have actual exit strategy
		while (! _rpc.is_closed() && _running ) {
			// We need to start a worker on the next request.
			// Either the request has been stored and we use the inbox
			// or we wait for the next one from the RPC interface.
			
			if (! _inbox.empty())
			{
				started = _process_input_message(_inbox.back());
				_inbox.pop();
			}
			else 
			{
				started = _process_input_message(_rpc.get());
			}

			// If the worker has not been actually started (cancelled before start)
			// for example, then relaunch the start process.
			if(! started)
				continue;

			_wait_for_worker_start();

			// Now, we need to actually handle the incoming commands
			while(_worker_running)
			{
				while(_worker_running && ! _rpc.have_data())
				{
					// If no data, wait for 0.1s and re-check the worker
					std::this_thread::sleep_for(100ms);
				}

				_handle_next_incoming_message();
			}
			spdlog::debug("Worker is done, waiting for an actual function for a restart.");
		}
		_rpc.close();
		spdlog::info("Dispatcher exiting.");
	}

	void LSPCommandDispatcher::_handle_next_incoming_message()
	{
		// We don't want too block here, so if no data available, exit.
		if(! _rpc.have_data())
			return;
		// Here we have either a new function call (cancelRequest included) or a 
		// ResponseMessage. 
		// Requests impose the presence of "method", otherwise it is a reply.
		//
		// ID is *not* mandatory, as you may have a notification
		json new_call = _rpc.get();
		spdlog::debug("Handling of {}",new_call.dump());


		
		json tgt_id;
		if(new_call.contains("id"))
			tgt_id = new_call.at("id");
		
		std::string str_tgt_id = "";

		if( tgt_id.is_number())
			str_tgt_id = std::to_string(int(tgt_id));
		else if (tgt_id.is_null())
			str_tgt_id = "";
		else
			str_tgt_id = tgt_id;
		const std::string method_name = new_call.value("method","");
		
		if( method_name == "$/cancelRequest")
		{
			if(_ongoing_id == tgt_id)
			{
				spdlog::info("Request cancellation of current worker with id {}",str_tgt_id);
				_finish_worker();
				return;
			}
			else 
			{
				spdlog::info("Store cancellation of call {}",str_tgt_id);
				_cancelled_calls.emplace(str_tgt_id);
			}
		}
		else if (! tgt_id.is_null() && str_tgt_id == _awaited_id.value_or("")) 
		{
			if( new_call.contains("result"))
			{
				// Handle the result of a request that has been sent by the worker
				spdlog::debug("Got result for {}",str_tgt_id);
				_cb_data.set_value(new_call.at("result"));												
			}
			else if (new_call.contains("error")) 
			{
				spdlog::debug("Got error for {}",str_tgt_id);

				_cb_data.set_exception(std::make_exception_ptr( 
					rpc_base_exception(new_call.at("error")) ));
			}
			else 
			{
				_cb_data.set_exception(std::make_exception_ptr(
					rpc_invalid_request_error(
						fmt::format("Received call for in-use id {} but without response payload.",str_tgt_id),
						new_call
					)));
			}
		}
		else if (! method_name.empty()) 
		{
			// Store the call in the inbox if it cannot be generated 
			_inbox.push(new_call);
		}
	
	}

	void LSPCommandDispatcher::_finish_worker()
	{
		if(_worker_running)
		{
			std::unique_lock lk(_work_mutex);
			_worker_cv.wait(lk,[this]{return ! _worker_running; });
		}
	}
	
	
	void LSPCommandDispatcher::_wait_for_worker_start()
	{
		if(! _worker_running)
		{
			std::unique_lock lk(_work_mutex);
			_worker_cv.wait(lk,[this]{return _worker_running; });
		}
	}

	void LSPCommandDispatcher::bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_requests.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);
		spdlog::info("Binding handler for request {}", fct_name);
        _bound_requests[fct_name] = cb;
    }

    void LSPCommandDispatcher::bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_notifs.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);
     	spdlog::info("Binding handler for notif   {}", fct_name);
		_bound_notifs[fct_name] = cb;
    }

	void LSPCommandDispatcher::bind_call_filter(std::function< bool(const std::string&, const json&)> filter)
	{
		_bound_filter = filter;
	}

	void LSPCommandDispatcher::_send_rpc_call(const std::string& method, const json& params,  const std::optional<std::string> id)
	{
		json to_send;
		to_send["method"] = method;
		to_send["params"] = params;

		if(id)
			to_send["id"] = id.value();
		_rpc.send(to_send);

	}

	std::string LSPCommandDispatcher::_send_rpc_call(const std::string& method, const json& params)
	{
		std::string id =  uuids::to_string(_uuid());
		_send_rpc_call(method, params,id);
		return id;
	}


	bool LSPCommandDispatcher::_process_input_message(const json& data)
	{
		//If we have an invalid-ish call, just discard it. 
		// Warning, 'params' and id are not mandatory.
		if (! data.contains("method")) {
			spdlog::debug("Discard invalid RPC call {}", data.dump(2));
			return false;
		}

		// Flush the input, in order to get potential cancel request before starting the command.
		while(_rpc.have_data())
			_handle_next_incoming_message();

		std::string method = data.at("method");


		json id;
		if(data.contains("id"))
			id = data.at("id");
		
		std::string str_id = "";

		if( id.is_number())
			str_id = std::to_string(int(id));
		else if (id.is_null())
			str_id = "";
		else
			str_id = id;

		if (_cancelled_calls.contains(str_id)) 
		{
			_cancelled_calls.erase(str_id);
			return false;
		}

		
		json params;

		if(data.contains("params"))
		{
			// Redirect call of execute command to the underlying command call
			if(method == "workspace/executeCommand")
			{
				types::ExecuteCommandParams ecp = data.at("params");
				method = ecp.command;
				params = ecp.arguments;
			}
			else
			{
				params = data.at("params");
			}

			if(_unpack_non_standard_args && params.is_array() && is_non_standard_method(method))
			{
				if(params.size() == 0)
				{
					params = json();
				}
				else if(params.size() == 1)
				{
					spdlog::debug("Unpacking params array for {}", method);
					params = params.at(0);
				}
				else 
					spdlog::debug("Impossible to unpack param array for method {}, too much elements ({:d})", method, params.size());					
			}
		}

		{
			std::unique_lock lk(_work_mutex);
			_ongoing_id = id;
			_ongoing_method = method;
			_ongoing_params = params;
			_invoke();
		}
		_worker_cv.notify_all();
		// Initiate the worker call 
		// Ongoing_id and other related variables shall be reset 
		// by the end of the worker job.
		return true;
		
	}

	void LSPCommandDispatcher::_invoke()
	{
		spdlog::info("Invoke command {}",_ongoing_method);
		if(! is_bound(_ongoing_method))
			forward_exception(rpc_method_not_found_error(_ongoing_method));

		else
		{
			_worker_running = true;
			// --------------------------------------------------------
			// Start of the worker thread
			_worker = std::jthread([this](std::stop_token tk){

				// Start by acquiring the MUTEX to block all others from accessing
				// current process variables.
				
				std::lock_guard<std::mutex> lk(_work_mutex);
				spdlog::info("Started worked for {}", _ongoing_method);
				try {
					if(_bound_filter.has_value() && _bound_filter.value()(_ongoing_method,_ongoing_params))
					{
						if(is_notif(_ongoing_method))
						{
							_bound_notifs[_ongoing_method](_ongoing_params,tk);	
							
							// For non-standard *notifications*, a return value SHALL be sent back
							// as the initiating call is workspace/executeCommand which is a request.
							if(is_non_standard_method(_ongoing_method))
								forward_result(nullptr);
						}
						else
						{
							forward_result(_bound_requests[_ongoing_method](_ongoing_params, tk));
						}
					}
					else 
					{
						spdlog::info("Method call of {} has been filtered out", _ongoing_method);
					}
				} catch (const client_cancel_request_exception& e) {
					spdlog::info("Method {} was stopped by client request", _ongoing_method);
				} catch (const rpc_base_exception& e) {
					forward_exception(e);
				} catch (const std::exception& e) {
					// If any unhandled exception raises during a call to an execute command, 
					// Just rethrow as an unknown error.
					spdlog::error("Got unknown error during the handling of {}: {}",_ongoing_method, e.what());
					spdlog::debug("Arguments were: {}",_ongoing_params.dump(1));
					forward_exception(lsp_unknown_error(e.what()));

					_ongoing_id = json(nullptr);
					_worker_running = false;
					_worker_cv.notify_all();
				}
				spdlog::debug("Worker finished {} [{}]",_ongoing_method, _ongoing_id.dump());
				_ongoing_id = json(nullptr);
				_worker_running = false;
				_worker_cv.notify_all();
			});
			// End of the worker thread
			// --------------------------------------------------------
		}
	}

	void LSPCommandDispatcher::forward_exception(const rpc_base_exception& e)
	{
		json ret;
		if( ! _ongoing_id.is_null())
			ret["id"] = _ongoing_id;
		else 
			ret["id"] = nullptr;

		ret["error"] = e;

		_rpc.send(ret);
	}

	void LSPCommandDispatcher::forward_result(const nlohmann::json& val)
	{
		json ret;
		if(! _ongoing_id.is_null())
			ret["id"] = _ongoing_id;
		else 
			ret["id"] = nullptr;

		ret["result"] = val;

		_rpc.send(ret);
	}

	void LSPCommandDispatcher::notify_client(const std::string& method, const json& args)
	{
		_send_rpc_call(method, args, {});
	}

	std::future<json> LSPCommandDispatcher::request_client_future(const std::string& method, const json& args)
	{
		// Notification and requests have the same format aside from the return value.
		_awaited_id = _send_rpc_call(method, args);
		_cb_data = std::promise<json>();
		return _cb_data.get_future();
	}

	json LSPCommandDispatcher::request_client(const std::string& method, const json& args)
	{
		using namespace std::chrono_literals;
		auto f = request_client_future(method, args);
		
		std::optional<std::stop_token> tk;
		if(std::this_thread::get_id() == _worker.get_id())
			tk = _worker.get_stop_token();

		if(_client_timeout || (tk.has_value() && ! tk->stop_requested()))
		{
			const auto call_time = std::chrono::system_clock::now();
			const auto timeout = _client_timeout > 0 ? (call_time + std::chrono::milliseconds(_client_timeout)) : call_time.max();
			
			while (f.wait_for(100ms) == std::future_status::timeout)
			{
				if(tk.has_value() && tk->stop_requested())
					throw client_cancel_request_exception("Request wait cancelled by client");
				if(std::chrono::system_clock::now() > timeout)
					throw client_timeout_error(fmt::format("Client timed out on method {}. Timeout was {} ms", method, _client_timeout));
			}
		}	
		else 
		{
			f.wait();
		}
	
		return f.get();
	}

	void LSPCommandDispatcher::transfer_supported_nonstandard_methods(std::vector<std::string>& tgt) const
	{
		for (const auto& [name,_] : _bound_notifs)
			tgt.push_back(name);
		for (const auto& [name,_] : _bound_requests)
			tgt.push_back(name);
	}

	bool LSPCommandDispatcher::is_notif(const std::string &fct) const
    {
        return _bound_notifs.contains(fct);
    }

    bool LSPCommandDispatcher::is_request(const std::string &fct) const
    {
        return _bound_requests.contains(fct);
    }

    bool LSPCommandDispatcher::is_bound(const std::string &fct) const
    {
        return is_notif(fct) || is_request(fct);
    }

    bool LSPCommandDispatcher::is_non_standard_method(const std::string &fct) const
    {
		return is_bound(fct) && ! types::RESERVED_METHODS.contains(fct);
    }
}
