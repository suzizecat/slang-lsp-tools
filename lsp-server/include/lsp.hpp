/**
 * @file lsp.hpp
 * @author Julien FAUCHER (suzizecat@free.fr)
 * @brief Describe the core of a LSP server
 * @version 0.0.1
 * @date 2023-08-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once

#include "nlohmann/json.hpp"
#include "rpc_transport.hpp"

#include <unordered_map>
#include <functional>
#include <optional>

#include "types/enums/TraceValues.hpp"
#include "types/structs/ServerCapabilities.hpp"
#include "lsp_errors.hpp"

#include <istream>
#include <ostream>
#include <iostream>

namespace slsp{
    using json = nlohmann::json;

    /**
     * @brief Base LSP class
     * 
     * This class implement the boilerplate for a LSP.
     * It takes into account the RPC side and various utilities.
     */
    class BaseLSP
    {
    protected:
        bool _is_initialized;
        bool _is_stopping;
        bool _is_stopped;

        /**
         * @brief Registered trace level 
         * 
         * Should be taken into consideration  when the server sends to the client
         * a trace notification.
         */
        types::TraceValues _trace_level;


        rpc::RPCPipeTransport _rpc; 

        std::unordered_map<std::string, std::function<json(BaseLSP*,json&)>> _bound_requests;
        std::unordered_map<std::string, std::function<void(BaseLSP*,json&)>> _bound_notifs;

        void _filter_invocation(const std::string& fct_name) const;
        
    public:
        /**
         * @brief Construct a new BaseLSP object
         * 
         * @param is Input data stream (from client to server)
         * @param os Output data stream (from server to client)
         */
        explicit BaseLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        void bind_request(const std::string& fct_name, std::function<json(BaseLSP*,json&)> cb, bool allow_override = false);
        void bind_notification(const std::string& fct_name, std::function<void(BaseLSP*,json&)> cb, bool allow_override = false);
        json invoke_request(const std::string& fct_name, json& args);
        void invoke_notif(const std::string& fct_name, json& args);
        std::optional<json> invoke(const std::string& fct, json& params);

        bool is_notif(const std::string& fct) const;
        bool is_request(const std::string& fct) const;
        bool is_bound(const std::string& fct) const;

        void set_trace_level(const types::TraceValues level);

        inline void shutdown() {_is_stopping = true;};
        inline void exit() {_is_stopped = true;};

        void trace(const std::string& message, const std::string verbose = "");
        void send_notification(const std::string& fct, nlohmann::json && params = json());
        void run();

        types::ServerCapabilities capabilities;
        
    };

    typedef std::function<json(BaseLSP*,json&)> request_handle_t;
    typedef std::function<void(BaseLSP*,json&)> notification_handle_t;
}
