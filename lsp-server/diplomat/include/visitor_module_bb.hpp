#pragma once

#include "slang/syntax/SyntaxVisitor.h"
#include "slang/syntax/AllSyntax.h"
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

using json = nlohmann::json;

struct ModuleParam
{
    std::string name;
    std::string default_value;
    std::string type;
};


struct ModulePort
{
	std::string name;
	std::string size;
	std::string type;
	std::string direction;
	bool is_interface;
	std::string modport;
};

struct ModuleBlackBox
{
    std::string module_name;
    std::vector<ModuleParam> parameters;
    std::vector<ModulePort> ports;
};

    void to_json(json& j, const ModuleParam& p);
    void to_json(json& j, const ModulePort& p);
    void to_json(json& j, const ModuleBlackBox& p);

    void from_json(const json& j, ModuleParam& p);
    void from_json(const json& j, ModulePort& p);
    void from_json(const json& j, ModuleBlackBox& p);

class VisitorModuleBlackBox : public slang::syntax::SyntaxVisitor<VisitorModuleBlackBox>
{

protected:
        bool _only_modules;
        const slang::SourceManager* _sm;

public:
        explicit VisitorModuleBlackBox(bool only_modules = false, const slang::SourceManager* sm = nullptr);
        void handle(slang::syntax::ModuleHeaderSyntax &node);
        void handle(slang::syntax::NonAnsiPortSyntax &port);
        void handle(slang::syntax::NonAnsiPortListSyntax &port);
        void handle(slang::syntax::ImplicitAnsiPortSyntax &port);
        void handle(slang::syntax::ParameterDeclarationSyntax &node);

        void set_source_manager(const slang::SourceManager *new_sm);

		// std::string module_name;
		ModuleBlackBox bb;
};
