#include "lsp_errors.hpp"
#include "nlohmann/json_fwd.hpp"

namespace slsp {
    void to_json(nlohmann::json& j, const rpc_base_exception& e)
    {
        j = nlohmann::json({ {"code",e.code()},{"message",e.msg()} });
        if (e.data().has_value())
            j["data"] = e.data().value();
    }

    rpc_base_exception::rpc_base_exception(const nlohmann::json& j) : 
        _code(j.at("code").template get<int>()),
        _message(j.at("message").template get<std::string>()),
        _data(j.value("data",nullptr))
        {}
    
}
