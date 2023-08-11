#include "lsp.hpp"
#include <iostream>
#include "spdlog/spdlog.h" 
#include "lsp_errors.hpp"
using json = nlohmann::json;

namespace slsp{
    BaseLSP::BaseLSP() : 
    _is_stopping(false),
    _is_stopped(false),
    _trace_level(types::TraceValues::TraceValues_Off),
    _is_initialized(false),
    _rpc(std::cin, std::cout),
    _bound_requests(),
    _bound_notifs()
    {}
                        
    void BaseLSP::bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_requests.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);

        _bound_requests[fct_name] = cb;
    }

    void BaseLSP::bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override)
    {
        if(! allow_override && _bound_notifs.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);

        _bound_notifs[fct_name] = cb;
    }

    json BaseLSP::invoke_request(const std::string& fct, json& params)
    {

        return _bound_requests[fct](this,params);
    }

    void BaseLSP::invoke_notif(const std::string& fct, json& params)
    {
        spdlog::info("Got notification {}",fct);

        return _bound_notifs[fct](this,params);
    }

    std::optional<json> BaseLSP::invoke(const std::string& fct, json& params)
    {
        if(is_request(fct))
            return invoke_request(fct,params);
        else if (is_notif(fct))
        {
            invoke_notif(fct,params);
            return {};
        }

        spdlog::warn("Call requested for unknown method {}", fct);
        throw rpc_method_not_found_error(fct);
        
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

    void BaseLSP::run()
    {
        while(! _rpc.is_closed() && ! _is_stopped)
        {
            // Reset all parsed content
            json ret = json();
            json params = json();
            std::string id = "";
            std::string method = "";
            
            std::optional<json> fct_ret;
            fct_ret.reset();
            
            bool require_send = false;
            
            try
            {
                json raw_input = _rpc.get();
                if (!raw_input.contains("id"))
                {
                    spdlog::error("Missing id attribute in recieved request. Discarding.");
                    throw rpc_invalid_request_error("Missing id attribute in recieved request.");
                }

                if(! raw_input.contains("method"))
                {
                    spdlog::error("Missing method attribute in recieved request. Discarding.");
                    throw rpc_invalid_request_error("Missing method attribute in recieved request.");
                }

                method = raw_input["method"].template get<std::string>();

                if (raw_input["id"].is_string())
                    id = raw_input["id"].template get<std::string>();
                else if (raw_input["id"].is_number())
                {
                    id = std::to_string(raw_input["id"].template get<int>());
                }
                else
                {
                    throw rpc_invalid_request_error("Invalid ID format: \"id\": " + raw_input["id"].dump());
                }


                if (raw_input.contains("params"))
                {
                    params = raw_input["params"];
                }
                
                fct_ret = invoke(method,params);
                
                if (fct_ret.has_value())
                {
                    ret["result"] = fct_ret.value();
                    require_send = true;
                }
            }
            catch(rpc_base_exception e)
            {
                ret["error"] = e;
                require_send = true;
            }

            if(require_send)
            {
                ret["id"] = id;
                ret["jsonrpc"] = "2.0";
                _rpc.send(ret);
            }
        }
    }
}