#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <regex>
#include <filesystem>

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

	struct DiplomatLSPIncludeDirs
	{
		std::vector<std::string> user;
		std::vector<std::string> system;
	};

	struct DiplomatLSPWorkspaceSettings
	{
		std::unordered_set<std::filesystem::path> workspace_dirs;
		std::unordered_set<std::string> excluded_paths;
		std::unordered_set<std::string> excluded_patterns;
		
		std::vector<SlangDiagDesignator> ignored_diagnostics;
		std::vector<std::regex> excluded_regexs;
		std::optional<std::string> top_level;

		DiplomatLSPIncludeDirs includes;

		void refresh_regexs();
	};
	
	void to_json(nlohmann::json& j, const SlangDiagDesignator& s);
	void from_json(const nlohmann::json& j, SlangDiagDesignator& s); 

	void to_json(nlohmann::json& j, const DiplomatLSPIncludeDirs& s);
	void from_json(const nlohmann::json& j, DiplomatLSPIncludeDirs& s); 

	void to_json(nlohmann::json& j, const DiplomatLSPWorkspaceSettings& s);
	void from_json(const nlohmann::json& j, DiplomatLSPWorkspaceSettings& s); 

} // namespace slsp
