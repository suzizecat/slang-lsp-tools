
#include "lsp.hpp"
#include <iostream>
#include <utility>
#include "spdlog/spdlog.h" 
#include "lsp_errors.hpp"
#include "types/structs/LogTraceParams.hpp"
#include "types/structs/LogMessageParams.hpp"
#include "types/structs/ShowMessageParams.hpp"
#include "types/structs/ExecuteCommandParams.hpp"
#include "types/methods/lsp_reserved_methods.hpp"
#include <random>


using json = nlohmann::json;

namespace slsp{
    BaseLSP::BaseLSP(std::istream& is, std::ostream& os) : 
    _is_stopping(false),
    _is_stopped(false),
    _trace_level(types::TraceValues::TraceValues_Off),
    _is_initialized(false),
    _rpc(is,os),
    _bound_requests(),
    _bound_notifs(),
    _uuid(&_rand_engine),
    capabilities()
    {
        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size> {};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        _rand_engine = std::mt19937(seq);
    }
                        
    void BaseLSP::_filter_invocation(const std::string& fct_name) const
    {
        if(! _is_initialized)
        {
            if(fct_name != "initialize")
            {
                throw lsp_server_not_initialized_error();
            }
        }
        else if( _is_stopping)
        {
            if (fct_name != "exit")
            {
                throw rpc_invalid_request_error("Request invalid due to server shutting down.");
            }
        }
    }

    void BaseLSP::_register_custom_command(const std::string& fct_name)
    {
        if(! capabilities.executeCommandProvider.has_value())
            capabilities.executeCommandProvider = types::ExecuteCommandOptions();
        
        spdlog::info("Register new command {}",fct_name);
        capabilities.executeCommandProvider.value().commands.push_back(fct_name);
    }

    void BaseLSP::bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_requests.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);
        if(! types::RESERVED_METHODS.contains(fct_name))
            _register_custom_command(fct_name);
        _bound_requests[fct_name] = cb;
    }

    void BaseLSP::bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_notifs.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);
        if(! types::RESERVED_METHODS.contains(fct_name))
            _register_custom_command(fct_name);
        _bound_notifs[fct_name] = cb;
    }

    void BaseLSP::bind_callback(const std::string& id, notification_handle_t cb, bool allow_override)
    {
        _bound_callbacks[id] = cb;
    }

    void BaseLSP::_run_callback(const std::string& id, json& params)
    {
        if(! _bound_callbacks.contains(id))
            return;

        spdlog::info("Got callback for id {}",id);

        auto cb = _bound_callbacks.extract(id);
        cb.mapped()(params);
    }


    json BaseLSP::_invoke_request(const std::string& fct, json& params)
    {
        spdlog::info("Got request {}",fct);
        return _bound_requests[fct](params);
    }

    void BaseLSP::_invoke_notif(const std::string& fct, json& params)
    {
        spdlog::info("Got notification {}",fct);

        return _bound_notifs[fct](params);
    }

    std::optional<json> BaseLSP::invoke(const std::string& fct,  json& params)
    {
        _filter_invocation(fct);
        if (is_request(fct))
            return _invoke_request(fct,params);
        else if (is_notif(fct))
        {
            _invoke_notif(fct,params);
            return {};
        }

        spdlog::warn("Call requested for unknown method {}", fct);
        throw rpc_method_not_found_error(fct);
        
    }

    json BaseLSP::_execute_command_handler(json& p)
    {
        try
        {
            const types::ExecuteCommandParams params = p;
            json args = params.arguments.value_or(json());
            json ret = invoke(params.command, args).value_or(json());
            spdlog::trace("Invocation return is {}",ret.dump(1));
            return ret;
        }
        catch(const rpc_base_exception& e) { throw e; }
        catch(const server_side_base_exception& e) { throw e; }
        catch(const std::exception& e)
        {
            // If any unhandled exception raises during a call to an execute command, 
            // Just rethrow as an unknown error.
            spdlog::error("Got unknown error : {}",e.what());
            throw lsp_unknown_error(e.what());
        }        
    }

    bool BaseLSP::is_notif(const std::string &fct) const
    {
        return _bound_notifs.contains(fct);
    }

    bool BaseLSP::is_request(const std::string &fct) const
    {
        return _bound_requests.contains(fct);
    }

    bool BaseLSP::is_bound(const std::string &fct) const
    {
        return is_notif(fct) || is_request(fct);
    }

    void BaseLSP::set_trace_level(const types::TraceValues level)
    {
        if (level != types::TraceValues__TraceValues_Invalid)
            _trace_level = level;
    }

    void BaseLSP::trace(const std::string &message, const std::string verbose)
    {

        types::LogTraceParams params;
        if(_trace_level == types::TraceValues::TraceValues_Off)
            return;
        
        params.message= message;

        if(_trace_level > types::TraceValues_Messages)
            params.verbose = verbose;


        send_notification("$/logTrace",params);
    }

    void BaseLSP::log(const types::MessageType level, const std::string& message)
    {

        types::LogMessageParams params;
        params.type = level;        
        params.message= message;

        send_notification("window/logMessage",params);
    }

    void BaseLSP::show_message(const types::MessageType level, const std::string& message)
    {

        types::ShowMessageParams params;
        params.type = level;        
        params.message= message;

        send_notification("window/showMessage",params);
    }

    void BaseLSP::send_notification(const std::string &fct, nlohmann::json&& params)
    {
        json to_send;
        to_send["jsonrpc"] = "2.0";
        to_send["method"] = fct;
        if(! params.is_null())
            to_send["params"] = params;
        
        _rpc.send(to_send);
    }


    // Send a request to the client and bind a callback to the ID.
    void BaseLSP::send_request(const std::string &fct,std::function<void(json&)> cb, nlohmann::json &&params)
    {
        json to_send;
        std::string req_id = uuids::to_string(_uuid());
        to_send["jsonrpc"] = "2.0";
        to_send["method"] = fct;
        to_send["id"] = req_id;
        if(! params.is_null())
            to_send["params"] = params;

        spdlog::info("Sending request {} with id {}", fct, req_id);
        bind_callback(req_id, cb);
        _rpc.send(to_send);
    }

    void BaseLSP::run()
    {
        std::optional<std::string> id;
        while(! _rpc.is_closed() && ! _is_stopped)
        {
            // Reset all parsed content
            json ret = json();
            json params = json();
            bool has_id = false;
            bool is_method_call = false;
            id.reset();
            std::string method = "";
            
            std::optional<json> fct_ret;
            fct_ret.reset();
            
            bool require_send = false;
            
            try
            {
                json raw_input = _rpc.get();
                has_id = raw_input.contains("id");
                is_method_call = raw_input.contains("method");

                if(! is_method_call && ! has_id)
                {
                    spdlog::error("Missing method attribute in recieved request. Discarding.");
                    throw rpc_invalid_request_error("Missing method attribute in recieved request.");
                }

                if(has_id)
                {
                    if (raw_input["id"].is_string())
                    {
                        ret["id"] = raw_input["id"];
                        id = raw_input["id"].template get<std::string>();
                    }
                    else if (raw_input["id"].is_number())
                    {
                        ret["id"] = raw_input["id"];
                        id = std::to_string(raw_input["id"].template get<int>());
                    }
                    else
                    {
                        throw rpc_invalid_request_error("Invalid ID format: \"id\": " + raw_input["id"].dump());
                    }
                }

                if(is_method_call)
                {
                    method = raw_input["method"].template get<std::string>();

                    if (is_request(method))
                    {
                        if (!has_id)
                        {
                            spdlog::error("Missing id attribute in recieved request. Discarding.");
                            throw rpc_invalid_request_error("Missing id attribute in recieved request.");
                        }
                    }

                    if (raw_input.contains("params"))
                    {
                        params = raw_input["params"];
                    }
                    
                    fct_ret = invoke(method,params);
                }
                else if(has_id)
                {
                    // Might be the return from a server initiated request.
                    if(_bound_callbacks.contains(id.value()))
                    {
                        spdlog::debug("Processing callback {}",raw_input.dump(1));
                        _run_callback(id.value(), raw_input["result"]);
                    }
                    else
                    {
                        throw rpc_invalid_request_error("Got a callback with an id number that is not expected.");
                    }
                }
                
                if (fct_ret.has_value())
                {
                    ret["result"] = fct_ret.value();
                    require_send = true;
                }
            }
            catch (rpc_base_exception e)
            {
                spdlog::error("Catched rpc_base_exception: {}",e.what());
                ret["error"] = e;
                require_send = true;


            }
            catch (client_closed_exception e)
            {
                require_send = false;
                spdlog::warn("Client closed the connexion. The server will now exit.");
            }
            catch (server_side_base_exception e)
            {
                require_send = false;
                spdlog::error("Unexpected server side error: {}", e.what());
            }

            if(require_send && has_id)
            {
                // It is *required* to send back the ID with the same type (integer or string)
                // as it was recieved.
                // Here, the ID is set much above directly.
                // if(id)
                //     ret["id"] = id.value();
                spdlog::trace("Sending back {}",ret.dump(1));
                ret["jsonrpc"] = "2.0";
                _rpc.send(ret);
            }
        }
    }
}
