#include "lsp.hpp"
#include <iostream>
using json = nlohmann::json;

namespace slsp{
    BaseLSP::BaseLSP() : _is_initialized(false),
                         _rpc(std::cin, std::cout),
                         _bound_functions()
                         {}
                        
    void BaseLSP::bind(std::string &fct_name, std::function< json(json)> cb)
    {
        if(_bound_functions.contains(fct_name))
            throw std::runtime_error("Tried to add a callback to an already handled request " + fct_name);

        _bound_functions[fct_name] = cb;
    }

    json BaseLSP::_default_handler(json data)
    {
        throw method_not_found_exception();
    }

    void BaseLSP::_dispatch()
    {
        while(! _rpc.is_closed())
        {
            json raw_input = _rpc.get();
            json ret = json();
            ret["id"] = raw_input["id"];
            ret["jsonrpc"] = "2.0";
            
            if (_bound_functions.contains(raw_input['method']))
            {
                json params = json();
                if (raw_input.contains("params"))
                {
                    params = raw_input["params"];
                }

                try
                {
                    ret["result"] = _bound_functions[raw_input['method']](params);
                }
                catch(json e)
                {
                    ret["error"] = e;
                }

                _rpc.send(ret);
            }
        }
    }
}