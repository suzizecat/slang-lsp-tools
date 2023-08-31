#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <optional>

#include "nlohmann/json.hpp"
#include "slang/diagnostics/Diagnostics.h"

namespace slsp
{

    typedef struct SlangDiagDesignator
    {
        uint16_t sub_system;
        uint16_t error_code;

        operator slang::DiagCode() const;
        SlangDiagDesignator(slang::DiagCode& code);
        SlangDiagDesignator() = default;
    } SlangDiagDesignator;

    struct DiplomatLSPWorkspaceSettings
    {
       std::unordered_set<std::string> excluded_paths;
       std::vector<SlangDiagDesignator> ignored_diagnostics;
    };
    
    void to_json(nlohmann::json& j, const SlangDiagDesignator& s);
    void from_json(const nlohmann::json& j, SlangDiagDesignator& s); 
    void to_json(nlohmann::json& j, const DiplomatLSPWorkspaceSettings& s);
    void from_json(const nlohmann::json& j, DiplomatLSPWorkspaceSettings& s); 

} // namespace slsp
