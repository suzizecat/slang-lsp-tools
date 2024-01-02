#include "diplomat_lsp_ws_settings.hpp"
#include "fmt/format.h"
#include <stdexcept>
namespace slsp
{

    SlangDiagDesignator::operator slang::DiagCode() const
    {
        if (sub_system >= slang::DiagSubsystem_traits::values.size())
            throw std::range_error(fmt::format("Invalid subsystem ID {:d}. Last subsystem is {:d}", sub_system, slang::DiagSubsystem_traits::values.size() - 1));
        return slang::DiagCode{slang::DiagSubsystem_traits::values[sub_system], error_code};
    }

    SlangDiagDesignator::SlangDiagDesignator(slang::DiagCode &code) : sub_system((uint16_t)code.getSubsystem()),
                                                                      error_code(code.getCode())
    {
    }

    void to_json(nlohmann::json &j, const SlangDiagDesignator &s)
    {
        j = nlohmann::json{{"subsystem", s.sub_system}, {"code", s.error_code}};
    }
    void from_json(const nlohmann::json &j, SlangDiagDesignator &s)
    {
        j.at("subsystem").get_to(s.sub_system);
        j.at("code").get_to(s.error_code);
    }

    void to_json(nlohmann::json &j, const DiplomatLSPWorkspaceSettings &s)
    {
        j = nlohmann::json{{"excludedPaths", s.excluded_paths}, {"excludedDiags", s.ignored_diagnostics}};
    }
    void from_json(const nlohmann::json &j, DiplomatLSPWorkspaceSettings &s)
    {
        j.at("excludedPaths").get_to(s.excluded_paths);
        j.at("excludedDiags").get_to(s.ignored_diagnostics);
    }
}