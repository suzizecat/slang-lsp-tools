
#include "lsp.hpp"
#include <fmt/format.h>
#include <iostream>
#include <stop_token>
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
    _stop_tk(*_dispatcher.get_stop_tk()),
    capabilities()
    {
        _dispatcher.bind_call_filter(std::bind(&BaseLSP::_filter_invocation, this, std::placeholders::_1, std::placeholders::_2));
    }
           

    void BaseLSP::run()
    {
        try {
            _dispatcher.run();
        } catch (const client_closed_exception& e) 
        {
            spdlog::info("Caught client closed exception, the server will shut down.");
        }
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

        return token;
    }

    bool BaseLSP::is_work_done_token_valid(const std::string& token) const
    {
        return _active_progress_tokens.contains(token);
    }

    bool BaseLSP::is_work_done_token_active(const std::string& token) const
    {
        return _active_progress_tokens.contains(token) && _active_progress_tokens.at(token);
    }

    void BaseLSP::begin_progress(const std::string& token, const types::WorkDoneProgressBegin& args)
    {
        if(is_work_done_token_valid(token))
        {
            send_notification("$/progress",types::ProgressParams{.token=token,.value=args});
            _active_progress_tokens[token] = true;
        }
    }

    void BaseLSP::report_progress(const std::string& token, const types::WorkDoneProgressReport& args)
    {
        if(is_work_done_token_active(token))
        {
            send_notification("$/progress",types::ProgressParams{.token=token,.value=args});
        }
    }

    void BaseLSP::end_progress(const std::string& token, const types::WorkDoneProgressEnd& args)
    {
        if(is_work_done_token_active(token))
            send_notification("$/progress",types::ProgressParams{.token=token,.value=args});

        if(is_work_done_token_valid(token))
            _active_progress_tokens.erase(token);
    }

    // void BaseLSP::run()
    // {
    //     std::optional<std::string> id;
    //     while(! _rpc.is_closed() && ! _is_stopped)
    //     {
    //         // Reset all parsed content
    //         json ret = json();
    //         json params = json();
    //         bool has_id = false;
    //         bool is_method_call = false;
    //         id.reset();
    //         std::string method = "";
            
    //         std::optional<json> fct_ret;
    //         fct_ret.reset();
            
    //         bool require_send = false;
            
    //         try
    //         {
    //             json raw_input = _rpc.get();
                
    //             if(raw_input.empty())
    //                 continue;

    //             has_id = raw_input.contains("id");
    //             is_method_call = raw_input.contains("method");

    //             if(! is_method_call && ! has_id)
    //             {
    //                 spdlog::error("Missing method attribute in recieved request. Discarding.");
    //                 throw rpc_invalid_request_error("Missing method attribute in recieved request.");
    //             }

    //             if(has_id)
    //             {
    //                 if (raw_input["id"].is_string())
    //                 {
    //                     ret["id"] = raw_input["id"];
    //                     id = raw_input["id"].template get<std::string>();
    //                 }
    //                 else if (raw_input["id"].is_number())
    //                 {
    //                     ret["id"] = raw_input["id"];
    //                     id = std::to_string(raw_input["id"].template get<int>());
    //                 }
    //                 else
    //                 {
    //                     throw rpc_invalid_request_error("Invalid ID format: \"id\": " + raw_input["id"].dump());
    //                 }
    //             }

    //             if(is_method_call)
    //             {
    //                 method = raw_input["method"].template get<std::string>();

    //                 if (is_request(method))
    //                 {
    //                     if (!has_id)
    //                     {
    //                         spdlog::error("Missing id attribute in recieved request. Discarding.");
    //                         throw rpc_invalid_request_error("Missing id attribute in recieved request.");
    //                     }
    //                 }

    //                 if (raw_input.contains("params"))
    //                 {
    //                     params = raw_input["params"];
    //                 }
    //                 spdlog::stopwatch sw;
    //                 fct_ret = invoke(method,params);
    //                 std::string cmd_name = method;
    //                 if(method == "workspace/executeCommand")
    //                     cmd_name += "/" + params["command"].template get<std::string>();
                    
    //                 spdlog::info("Method {} invocation done in {:.3}s",cmd_name,sw);
    //                 if(fct_ret)
    //                     spdlog::debug("Returned:\n{}",fct_ret.value().dump(1));
    //             }
    //             else if(has_id)
    //             {
    //                 // Might be the return from a server initiated request.
    //                 if(_bound_callbacks.contains(id.value()))
    //                 {
    //                     _CallbackContextHandler raii{.id=id.value(),.tgt=this};

    //                     spdlog::debug("Processing callback {}",raw_input.dump(1));
    //                     if(raw_input.contains("error"))
    //                     {
    //                         throw  server_side_base_exception(fmt::format("Client replied to request with an error {} : {}",
    //                             raw_input["error"]["code"].template get<int>(), 
    //                             raw_input["error"]["message"].template get<std::string>()
    //                         ));
    //                     }
    //                     else 
    //                     {
    //                         _run_callback(id.value(), raw_input["result"]);
    //                     }
    //                 }
    //                 else
    //                 {
    //                     throw rpc_invalid_request_error("Got a callback with an id number that is not expected.");
    //                 }
    //             }
                
    //             if (fct_ret.has_value())
    //             {
    //                 ret["result"] = fct_ret.value();
    //                 require_send = true;
    //             }
    //         }
    //         catch (rpc_base_exception e)
    //         {
    //             spdlog::error("Catched rpc_base_exception: {}",e.what());
    //             ret["error"] = e;
    //             require_send = true;


    //         }
    //         catch (client_closed_exception e)
    //         {
    //             require_send = false;
    //             spdlog::warn("Client closed the connexion. The server will now exit.");
    //         }
    //         catch (server_side_base_exception e)
    //         {
    //             require_send = false;
    //             spdlog::error("Unexpected server side error: {}", e.what());
    //         }

    //         if(require_send && has_id)
    //         {
    //             // It is *required* to send back the ID with the same type (integer or string)
    //             // as it was recieved.
    //             // Here, the ID is set much above directly.
    //             // if(id)
    //             //     ret["id"] = id.value();
    //             spdlog::trace("Sending back {}",ret.dump(1));
    //             ret["jsonrpc"] = "2.0";
    //             _rpc.send(ret);
    //         }
    //     }
    // }
}
