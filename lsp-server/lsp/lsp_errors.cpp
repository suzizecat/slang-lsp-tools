#include "lsp_errors.hpp"

namespace slsp {
    void to_json(nlohmann::json& j, const rpc_base_exception& e)
    {
        j = nlohmann::json({ {"code",e.code()},{"message",e.msg()} });
        const std::optional<nlohmann::json > add_data = e.data();
        if (add_data)
            j["data"] = add_data.value();
    }
}