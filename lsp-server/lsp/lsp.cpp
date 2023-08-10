#include "lsp.hpp"
#include <iostream>
#include "spdlog/spdlog.h" 
using json = nlohmann::json;

namespace slsp{
    BaseLSP::BaseLSP() : 
    _is_stopping(false),
    _is_initialized(false),
    _rpc(std::cin, std::cout),
    _bound_requests(),
    _bound_notifs()
    {}
                        
    void BaseLSP::bind_request(const std::string& fct_name, request_handle_t cb)
    {
        if(_bound_requests.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);

        _bound_requests[fct_name] = cb;
    }

    void BaseLSP::bind_notification(const std::string& fct_name, notification_handle_t cb)
    {
        if(_bound_notifs.contains(fct_name))
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

        throw std::runtime_error("Call to unbound method " + fct);
        
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

    void BaseLSP::run()
    {
        while(! _rpc.is_closed() && ! _is_stopped)
        {
            json raw_input = _rpc.get();
            bool require_send = false;
            json ret = json();
            std::optional<json> fct_ret;
            if(! raw_input.contains("method"))
            {
                spdlog::error("Missing method attribute in recieved request. Discarding.");
                continue;
            }
            std::string method = raw_input["method"].template get<std::string>();
            if (is_bound(method))
            {
                json params = json();
                if (raw_input.contains("params"))
                {
                    params = raw_input["params"];
                }

                try
                {

                    fct_ret = invoke(method,params);
                    
                    if (fct_ret.has_value())
                    {
                        ret["result"] = fct_ret.value();
                        require_send = true;
                    }

                }
                catch(json e)
                {
                    ret["error"] = e;
                    require_send = true;
                }

                if(require_send)
                {
                    ret["id"] = raw_input["id"];
                    ret["jsonrpc"] = "2.0";
                    _rpc.send(ret);
                }
            }
        }
    }
}