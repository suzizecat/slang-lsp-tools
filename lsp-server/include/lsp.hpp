#ifndef _H_LSP_DEF
#define _H_LSP_DEF

#include "nlohmann/json.hpp"
#include "rpc_transport.hpp"

#include <unordered_map>
#include <functional>
#include <optional>

namespace slsp{
    using json = nlohmann::json;

    class lsp_exception : public std::exception { };
    class method_not_found_exception : public std::exception { };

    class BaseLSP
    {
        bool _is_initialized;
        bool _is_shutdown;
        rpc::RPCPipeTransport _rpc;

        std::unordered_map<std::string, std::function<json(BaseLSP*,json&)>> _bound_requests;
        std::unordered_map<std::string, std::function<void(BaseLSP*,json&)>> _bound_notifs;
        
    public:
        explicit BaseLSP();

        void bind_request(const std::string& fct_name, std::function<json(BaseLSP*,json&)> cb);
        void bind_notification(const std::string& fct_name, std::function<void(BaseLSP*,json&)> cb);
        json invoke_request(const std::string& fct_name, json& args);
        void invoke_notif(const std::string& fct_name, json& args);
        std::optional<json> invoke(const std::string& fct, json& params);

        bool is_notif(const std::string& fct) const;
        bool is_request(const std::string& fct) const;
        bool is_bound(const std::string& fct) const;

        inline void shutdown() {_is_shutdown = true;};
        
        void run();
        
    };

    typedef std::function<json(BaseLSP*,json&)> request_handle_t;
    typedef std::function<void(BaseLSP*,json&)> notification_handle_t;
}
#endif  //_H_LSP_DEF