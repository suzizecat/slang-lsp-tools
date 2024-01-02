#pragma once

#include "lsp.hpp"
#include "nlohmann/json.hpp"

namespace slsp {
    namespace binds {
        void set_trace_level(BaseLSP *srv, nlohmann::json &params);
        void exit(BaseLSP *srv, nlohmann::json &params);

        nlohmann::json shutdown(BaseLSP *srv, nlohmann::json &params);
    }

    void perform_default_binds(BaseLSP &server);
}