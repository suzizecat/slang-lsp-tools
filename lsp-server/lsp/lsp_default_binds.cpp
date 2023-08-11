#include "lsp_default_binds.hpp"
#include "types/structs/SetTraceParams.hpp"
namespace slsp {
    using namespace types;
    using json = nlohmann::json;

    namespace binds
    {
        void set_trace_level(BaseLSP* srv, json& params)
        {
            SetTraceParams p = params;
            srv->set_trace_level(p.value);
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
        server.bind_notification("$/setTrace", binds::set_trace_level);
        server.bind_notification("exit", binds::exit);
        
        server.bind_request("shutdown", binds::shutdown);
    }
}