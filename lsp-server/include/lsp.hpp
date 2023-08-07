#ifndef _H_LSP_DEF
#define _H_LSP_DEF

#include "nlohmann/json.hpp"
#include "rpc_transport.hpp"

#include <unordered_map>
#include <functional>

namespace slsp{
    using json = nlohmann::json;


    class lsp_exception : public std::exception { };
    class method_not_found_exception : public std::exception { };

    class BaseLSP
    {
        bool _is_initialized;
        rpc::RPCPipeTransport _rpc;

        std::unordered_map<std::string, std::function<json(json)>> _bound_functions;
        
        void _dispatch();
        json _default_handler(json data);

    public:
        explicit BaseLSP();

        void bind(std::string &fct_name, std::function< json(json)> cb);
        
    };
}
#endif  //_H_LSP_DEF