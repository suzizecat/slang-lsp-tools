#include "lsp_dispatcher.hpp"
#include "lsp_errors.hpp"
#include "types/methods/lsp_reserved_methods.hpp"
#include <chrono>
#include <exception>
#include <future>
#include <spdlog/spdlog.h>
#include <stop_token>
#include <thread>



namespace diplomat::lsp {


	LSPCommandDispatcher::LSPCommandDispatcher(std::istream& is, std::ostream& os) : 
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
	_ongoing_id(""),
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
		// TODO Have actual exit strategy
		while (true) {
			// We need to start a worker on the next request.
			// Either the request has been stored and we use the inbox
			// or we wait for the next one from the RPC interface.
			if (! _inbox.empty())
			{
				_process_input_message(_inbox.back());
				_inbox.pop();
			}
			else 
			{
				_process_input_message(_rpc.get());
			}

			// If the worker has not been actually started (cancelled before start)
			// for example, then relaunch the start process.
			if(! _ongoing_id.has_value())
				continue;

			_wait_for_worker_start();

			// Now, we need to actually handle the incoming commands
			while(_worker_running)
			{
				if(! _rpc.have_data())
				{
					// If no data, wait for 0.1s and re-check the worker
					std::this_thread::sleep_for(100ms);
					continue;
				}

				_handle_next_incoming_message();

			spdlog::debug("Worker is done, waiting for an actual function for a restart.");
			}
		}
	}

	void LSPCommandDispatcher::_handle_next_incoming_message()
	{
		// We don't want too block here, so if no data available, exit.
		if(! _rpc.have_data())
			return;
		// Here we have either a new function call (cancelRequest included) or a 
		// ResponseMessage. 
		// Requests impose the presence of "method", otherwise it is a reply.
		json new_call = _rpc.get();


		if(! new_call.contains("id"))
		{
			spdlog::error("Invalid RPC object: no ID field provided:\n{}",new_call.dump(1));
			throw slsp::rpc_invalid_request_error("Received a RPC message without 'id' field");
		}

		
		const std::string tgt_id = new_call.at("id");
		const std::string method_name = new_call.value("method","");
		
		if( method_name == "$/cancelRequest")
		{
			if(_ongoing_id.value_or("") == tgt_id)
			{
				spdlog::info("Request cancellation of current worker with id {}",tgt_id);
				_finish_worker();
				return;
			}
			else 
			{
				spdlog::info("Store cancellation of call {}",tgt_id);
				_cancelled_calls.emplace(tgt_id);
			}
		}
		else if (tgt_id == _ongoing_id.value_or("")) 
		{
			if( new_call.contains("result"))
			{
				// Handle the result of a request that has been sent by the worker
				spdlog::debug("Got result for {}",tgt_id);
				_cb_data.set_value(new_call.at("result"));												
			}
			else if (new_call.contains("error")) 
			{
				spdlog::debug("Got error for {}",tgt_id);

				_cb_data.set_exception(std::make_exception_ptr( 
					slsp::rpc_base_exception(new_call.at("error")) ));
			}
			else 
			{
				_cb_data.set_exception(std::make_exception_ptr(
					slsp::rpc_invalid_request_error(
						fmt::format("Received call for in-use id {} but without response payload.",tgt_id),
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
        _bound_requests[fct_name] = cb;
    }

    void LSPCommandDispatcher::bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_notifs.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);
        _bound_notifs[fct_name] = cb;
    }

	std::string LSPCommandDispatcher::_send_rpc_call(const std::string& method, const json& params,  const std::string& id)
	{
		json to_send;
		to_send["method"] = method;
		to_send["params"] = params;
		to_send["id"] = id;
		_rpc.send(to_send);

		return id;
	}

	std::string LSPCommandDispatcher::_send_rpc_call(const std::string& method, const json& params)
	{
		return _send_rpc_call(method, params, uuids::to_string(_uuid()));
	}


	void LSPCommandDispatcher::_process_input_message(const json& data)
	{
		//If we have an invalid-ish call, just discard it. 
		// Warning, 'params' is not mandatory.
		if (! data.contains("method") || ! data.contains("id")) {
			spdlog::debug("Discard invalid RPC call {}", data.dump(2));
			return;
		}

		// Flush the input, in order to get potential cancel request before starting the command.
		while(_rpc.have_data())
			_handle_next_incoming_message();

		const std::string& method = data.at("method");
		const std::string& id = data.at("id");

		if (_cancelled_calls.contains(id)) 
		{
			_cancelled_calls.erase(id);
			return;
		}

		json params;

		if(data.contains("params"))
		{
			params = data.at("params");
			if(_unpack_non_standard_args && params.is_array() && is_non_standard_method(method))
			{
				if(params.size() == 1)
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
		// Initiate the worker call 

		// Ongoing_id should be reset by the end of the worker job.
		// _ongoing_id.reset();

		
	}

	void LSPCommandDispatcher::_invoke()
	{
		if(! is_bound(_ongoing_method))
			forward_exception(slsp::rpc_method_not_found_error(_ongoing_method));

		else
		{
			_worker_running = true;
			_worker_cv.notify_all();
			_worker = std::jthread([this](std::stop_token _){

				// Start by acquiring the MUTEX to block all others from accessing
				// current process variables.
				
				std::unique_lock lk(_work_mutex);
				try {
					
					if(is_notif(_ongoing_method))
					{
						_bound_notifs[_ongoing_method](_ongoing_params);	
					}
					else
					{
						forward_result(_bound_requests[_ongoing_method](_ongoing_params));
					}
				
				} catch (const slsp::rpc_base_exception& e) {
					forward_exception(e);
				} catch (const std::exception& e) {
					_ongoing_id.reset();
					_worker_running = false;
					_worker_cv.notify_all();

					throw e;
				}

				_ongoing_id.reset();
				_worker_running = false;
				_worker_cv.notify_all();

			});
		}
	}

	void LSPCommandDispatcher::forward_exception(const slsp::rpc_base_exception& e)
	{
		json ret;
		if( _ongoing_id.has_value())
			ret["id"] = _ongoing_id.value();
		else 
			ret["id"] = nullptr;

		ret["error"] = e;

		_rpc.send(ret);
	}

	void LSPCommandDispatcher::forward_result(const nlohmann::json& val)
	{
		json ret;
		if( _ongoing_id.has_value())
			ret["id"] = _ongoing_id.value();
		else 
			ret["id"] = nullptr;

		ret["result"] = val;

		_rpc.send(ret);
	}

	void LSPCommandDispatcher::notify_client(const std::string& method, const json& args)
	{
		_send_rpc_call(method, args);
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
		if (_client_timeout)
		{
			if(f.wait_for(std::chrono::milliseconds(_client_timeout)) == std::future_status::timeout)
				throw client_timeout_error(fmt::format("Client timed out on method {}. Timeout was {} ms", method, _client_timeout));
		}	
		else 
			f.wait();
		
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
		return (! is_bound(fct)) || slsp::types::RESERVED_METHODS.contains(fct);
    }
}
