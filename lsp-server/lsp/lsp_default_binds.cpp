#include "lsp_default_binds.hpp"
#include "types/structs/SetTraceParams.hpp"
#include "spdlog/spdlog.h"
namespace slsp {
    using namespace types;
    using json = nlohmann::json;

    namespace binds
    {
        void set_trace_level(BaseLSP* srv, json& params)
        {
            SetTraceParams p = params;
            spdlog::info("Set trace through params {}",params.dump());
            srv->log(MessageType::MessageType_Log,"Setting trace level to " + params.at("value").template get<std::string>());
            srv->set_trace_level(p.value);
        }

        void initialized_handler(BaseLSP* srv, json& params)
        {
            spdlog::info("Client initialization complete.");
        }

        json shutdown(slsp::BaseLSP* lsp, json& params)
        {
            lsp->shutdown();
            
            // Return empty value in case of success
            return json();
        }

        void exit(slsp::BaseLSP* lsp, json& params)
        {
            lsp->exit();
        }
    }

    void perform_default_binds(BaseLSP &server)
    {
        // server.bind_notification("$/setTraceNotification", binds::set_trace_level);
        // server.bind_notification("$/setTrace", binds::set_trace_level);

        // server.bind_notification("initialized", binds::initialized_handler);
        // server.bind_notification("exit", binds::exit);
        
        // server.bind_request("shutdown", binds::shutdown);
        // server.bind_request("workspace/executeCommand", std::bind(&BaseLSP::execute_command_handler, &server, std::placeholders::_1, std::placeholders::_2));
    }
}