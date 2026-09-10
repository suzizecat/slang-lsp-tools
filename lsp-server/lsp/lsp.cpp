
#include "lsp.hpp"
#include <chrono>
#include <fmt/format.h>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include "lsp_dispatcher.hpp"
#include "spdlog/spdlog.h" 
#include "spdlog/stopwatch.h"
#include "lsp_errors.hpp"
#include "types/structs/LogTraceParams.hpp"
#include "types/structs/LogMessageParams.hpp"
#include "types/structs/ProgressParams.hpp"
#include "types/structs/ShowMessageParams.hpp"
#include "types/structs/ExecuteCommandParams.hpp"
#include "types/methods/lsp_reserved_methods.hpp"
#include "types/structs/WorkDoneProgressCreateParams.hpp"

#include <utility>

using json = nlohmann::json;
using sysclk = std::chrono::system_clock;
// /**
//  * @brief Formatting function to bind 'uri' to fmtlib
//  * 
//  * @param u an uri instance
//  * @return auto the formatted uri
//  */
// std::string format_as(uri& u) { return u.to_string(); }

namespace diplomat::lsp{

    BaseLSP::BaseLSP(std::istream& is, std::ostream& os) : 
    _dispatcher(is,os),
    _is_stopping(false),
    _is_stopped(false),
    _trace_level(types::TraceValues::TraceValues_Off),
    _is_initialized(false),
    _notif_mutex(),
    _progress_sender(),
    _stop_tk(*_dispatcher.get_stop_tk()),
    capabilities(),
    _last_progress_sent(sysclk::now())
    {
        _dispatcher.bind_call_filter(std::bind(&BaseLSP::_filter_invocation, this, std::placeholders::_1, std::placeholders::_2));
    }
           

    void BaseLSP::run()
    {
        _progress_sender = std::jthread([this](std::stop_token tk){this->_progress_sender_handler(tk);});
        try {
            _dispatcher.run();
        } catch (const client_closed_exception& e) 
        {
            spdlog::info("Caught client closed exception, the server will shut down.");
        }
        _progress_sender.get_stop_source().request_stop();
        
    }
    
    void BaseLSP::bind_request(const std::string& fct_name, request_handle_t cb, bool allow_override)
    {
        _dispatcher.bind_request(fct_name, std::forward<request_handle_t>(cb),allow_override);
    }
    void BaseLSP::bind_notification(const std::string& fct_name, notification_handle_t cb, bool allow_override)
    {
        _dispatcher.bind_notification(fct_name, std::forward<notification_handle_t>(cb),allow_override);
    }

    bool BaseLSP::_filter_invocation(const std::string& fct_name, const json& _) const
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

        return true;
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
        _dispatcher.notify_client(fct,params);
    }


    // Send a request to the client and bind a callback to the ID.
    json BaseLSP::send_request(const std::string &method, nlohmann::json &&params)
    {
        return _dispatcher.request_client(method, params);
    }

    const std::string BaseLSP::create_progress_report()
    {
        std::string token = _dispatcher.get_uuid();
        types::WorkDoneProgressCreateParams params = {.token=token};
        // The result is not used here (no actual return value)
        send_request("window/workDoneProgress/create",params); 
        _active_progress_tokens[token] = false;
        return token;
    }

    bool BaseLSP::is_work_done_token_valid(const std::string& token) const
    {
        return _active_progress_tokens.contains(token);
    }

    bool BaseLSP::is_work_done_token_active(const std::string& token) const
    {
        return is_work_done_token_valid(token) && _active_progress_tokens.at(token);
    }

    void BaseLSP::begin_progress(const std::string& token, const types::WorkDoneProgressBegin& args)
    {
        if(is_work_done_token_valid(token))
        {
            {
                // Immediately sends the begin notification
                std::lock_guard<std::mutex> lk(_notif_mutex);
                send_notification("$/progress",types::ProgressParams{.token=token,.value=args});
                _active_progress_tokens[token] = true;
            }
        }
    }

    void BaseLSP::report_progress(const std::string& token, const types::WorkDoneProgressReport& args)
    {
        if(is_work_done_token_active(token))
        {
            {
                std::lock_guard<std::mutex> lk(_notif_mutex);
                // Use the scheduled sender to send updates
                _queued_progress[token] = args;
            }
            // send_notification("$/progress",types::ProgressParams{.token=token,.value=args});
        }
    }

    void BaseLSP::end_progress(const std::string& token, const types::WorkDoneProgressEnd& args)
    {
        {
            std::lock_guard<std::mutex> lk(_notif_mutex);
            if(is_work_done_token_active(token))
                send_notification("$/progress",types::ProgressParams{.token=token,.value=args});

            // Erase does not throw if element is missing.
            _active_progress_tokens.erase(token);
            _queued_progress.erase(token);
        }
    }

    void BaseLSP::_progress_sender_handler(std::stop_token tk)
    {
        using namespace std::literals::chrono_literals;

        while(! tk.stop_requested())
        {
            std::this_thread::sleep_for(250ms);
            
            {
                std::lock_guard<std::mutex> lk(_notif_mutex);
                std::unordered_map<std::string,json>::iterator prog;
                while((prog = _queued_progress.begin()) != _queued_progress.end())
                {
                    send_notification("$/progress",types::ProgressParams{.token=prog->first,.value=prog->second});
                    _queued_progress.erase(prog);
                }
            }
        }
    }
