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
#include "nlohmann/json_fwd.hpp"
#include "rpc_transport.hpp"
#include "lsp_dispatcher.hpp"

#include <climits>
#include <string>
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
#include "types/structs/WorkDoneProgressBegin.hpp"
#include "types/structs/WorkDoneProgressEnd.hpp"
#include "types/structs/WorkDoneProgressReport.hpp"

#include <istream>
#include <ostream>
#include <iostream>

namespace diplomat::lsp{
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

        /**
         * @brief Command dispatcher component, handling the effective command
         * selection and dispatch.
         *
         * This component actually makes the call to other functions that will be bound in the 
         * LSP. 
         */
        LSPCommandDispatcher _dispatcher;

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
        
        /** This map contains the requested workDoneTokens and their active state.*/
        std::unordered_map<std::string, bool> _active_progress_tokens;
        

        types::ClientCapabilities _client_capabilities;

        bool _filter_invocation(const std::string& fct_name, const json& args) const;


    public:
        /**
         * @brief Construct a new BaseLSP object
         * 
         * @param is Input data stream (from client to server)
         * @param os Output data stream (from server to client)
         */
        explicit BaseLSP(std::istream& is = std::cin, std::ostream& os = std::cout);

        void bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override = false);
        void bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override = false);
        // void bind_callback(const std::string& id, std::function<void(json&)> cb, bool allow_override = false);
        
        // bool is_notif(const std::string& fct) const;
        // bool is_request(const std::string& fct) const;
        // bool is_bound(const std::string& fct) const;
        // bool is_non_standard_command(const std::string& fct) const;

        void set_trace_level(const types::TraceValues level);
        void set_rpc_use_endl(const bool use_endl){_dispatcher.set_rpc_use_endl(use_endl);};

        inline void shutdown() {_is_stopping = true;};
        inline void exit() { _is_stopped = true; _dispatcher.stop(); };
        inline void set_initialized(const bool state) { _is_initialized = state; };

        void trace(const std::string& message, const std::string verbose = "");
        void log(const types::MessageType level, const std::string& message);
        void show_message(const types::MessageType level, const std::string& message);
        void send_notification(const std::string& fct, nlohmann::json && params = json());
        json send_request(const std::string& fct, nlohmann::json && params = json());
        
        const std::string create_progress_report();
        bool is_work_done_token_valid(const std::string& token) const;
        bool is_work_done_token_active(const std::string& token) const;
        void begin_progress(const std::string& token, const types::WorkDoneProgressBegin& args);
        void report_progress(const std::string& token, const types::WorkDoneProgressReport& args);
        void end_progress(const std::string& token, const types::WorkDoneProgressEnd& args);

        void run();

        types::ServerCapabilities capabilities;
        
    };

}
