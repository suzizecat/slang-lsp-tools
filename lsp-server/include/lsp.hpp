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
#include <vector>

// For adl_serializer<std::optional<T>>
#include "index_elements.hpp"

#include "types/enums/TraceValues.hpp"
#include "types/enums/MessageType.hpp"
#include "types/structs/ServerCapabilities.hpp"
#include "lsp_errors.hpp"
#include "uuid.h"

#include <istream>
#include <ostream>
#include <iostream>
#include <random>

#define LSP_MEMBER_BIND(base_cls, fct) std::bind(&base_cls::fct, this, std::placeholders::_1)

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

        std::mt19937 _rand_engine;
        uuids::uuid_random_generator _uuid;

        std::unordered_map<std::string, std::function<json(json&)>> _bound_requests;
        std::unordered_map<std::string, std::function<void(json&)>> _bound_notifs;
        std::unordered_map<std::string, std::function<void(json&)>> _bound_callbacks;

        void _filter_invocation(const std::string& fct_name) const;
        void _register_custom_command(const std::string& fct_name);

        json _execute_command_handler(json& p);

        virtual json _invoke_request(const std::string& fct_name, json& args);
        virtual void _invoke_notif(const std::string& fct_name, json& args);
        virtual void _run_callback(const std::string& id, json& args);
        
    public:
        /**
         * @brief Construct a new BaseLSP object
         * 
         * @param is Input data stream (from client to server)
         * @param os Output data stream (from server to client)
         */
        explicit BaseLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        void bind_request(const std::string& fct_name, std::function<json(json&)> cb, bool allow_override = false);
        void bind_notification(const std::string& fct_name, std::function<void(json&)> cb, bool allow_override = false);
        void bind_callback(const std::string& id, std::function<void(json&)> cb, bool allow_override = false);

        std::optional<json> invoke(const std::string& fct, json& params);
        

        bool is_notif(const std::string& fct) const;
        bool is_request(const std::string& fct) const;
        bool is_bound(const std::string& fct) const;

        void set_trace_level(const types::TraceValues level);
        void set_rpc_use_endl(const bool use_endl){_rpc.set_endl(use_endl);};

        inline void shutdown() {_is_stopping = true;};
        inline void exit() { _is_stopped = true; };
        inline void set_initialized(const bool state) { _is_initialized = state; };

        void trace(const std::string& message, const std::string verbose = "");
        void log(const types::MessageType level, const std::string& message);
        void show_message(const types::MessageType level, const std::string& message);
        void send_notification(const std::string& fct, nlohmann::json && params = json());
        void send_request(const std::string& fct, std::function<void(json&)> cb, nlohmann::json && params = json());
        void run();

        types::ServerCapabilities capabilities;
        
    };

    typedef std::function<json(json&)> request_handle_t;
    typedef std::function<void(json&)> notification_handle_t;
}
