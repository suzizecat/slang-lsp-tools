#include "diplomat_lsp_ws_settings.hpp"
#include "fmt/format.h"
#include <stdexcept>


namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json& j, const std::optional<T>& opt) {
            if (opt) {
              j = *opt; // this will call adl_serializer<T>::to_json which will
                        // find the free function to_json in T's namespace!
            } else {
                j = nullptr;
            }
        }

        static void from_json(const json& j, std::optional<T>& opt) {
            if (j.is_null()) {
                opt = std::optional<T>();
            } else {
                opt = j.template get<T>(); // same as above, but with
                                           // adl_serializer<T>::from_json
            }
        }
    };
}


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
        j = nlohmann::json{
            {"workspaceDirs", s.workspace_dirs},
            {"excludedPaths", s.excluded_paths},
            {"excludedPatterns", s.excluded_patterns},
            {"excludedDiags", s.ignored_diagnostics},
            {"topLevel", s.top_level}};
    }
    void from_json(const nlohmann::json &j, DiplomatLSPWorkspaceSettings &s)
    {
        j.at("workspaceDirs").get_to(s.workspace_dirs);
        j.at("excludedPaths").get_to(s.excluded_paths);
        j.at("excludedPatterns").get_to(s.excluded_patterns);
        j.at("excludedDiags").get_to(s.ignored_diagnostics);
        j.at("topLevel").get_to(s.top_level);

        s.refresh_regexs();
    }

    void DiplomatLSPWorkspaceSettings::refresh_regexs()
    {
        excluded_regexs.clear();
        for(const std::string& pat : excluded_patterns)
        {
            excluded_regexs.push_back(std::regex(pat));
        }
    }
}