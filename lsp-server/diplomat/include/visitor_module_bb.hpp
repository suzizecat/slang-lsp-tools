#pragma once

#include "slang/syntax/SyntaxVisitor.h"
#include "slang/syntax/AllSyntax.h"
#include "nlohmann/json.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <concepts>
#include <ranges>

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
    std::string comment;
};

struct ModuleBlackBox
{  
    std::string module_name;
    std::vector<ModuleParam> parameters;
    std::vector<ModulePort> ports;

    //TODO : Actually replace with array of ModuleBB to
    // allow signature computation and better binding.
    std::unordered_set<std::string> deps;
};


    // Defines the concept of a range... made of strings...
    template <typename R>
    concept StringRange = std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, std::string>;

    // Use the concept to allow "ranges" of strings as parameters.
    // Given a moduleBB, this should allow to detect if we have an equivalence in definition.
    template<StringRange Rparams, StringRange Rports>
    std::size_t bb_signature(const std::string& name, const Rparams& params, const Rports& ports)
    {
        using namespace std;
        size_t h = hash<string>{}(name);
        for(const auto& param : params)
            h ^= rotr(hash<string>{}(param),1);
        for(const auto& port : ports)
            h ^= rotr(hash<string>{}(port),2);

        return h;
    }
    
    std::size_t bb_signature(const ModuleBlackBox& bb);

    void to_json(json& j, const ModuleParam& p);
    void to_json(json& j, const ModulePort& p);
    void to_json(json& j, const ModuleBlackBox& p);
    void to_json(json& j, const  ModuleBlackBox* const & p);

    void from_json(const json& j, ModuleParam& p);
    void from_json(const json& j, ModulePort& p);
    void from_json(const json& j, ModuleBlackBox& p);

class VisitorModuleBlackBox : public slang::syntax::SyntaxVisitor<VisitorModuleBlackBox>
{

protected:
        bool _only_modules;
        std::unique_ptr<ModuleBlackBox> _bb;
        const slang::SourceManager* _sm;

public:
        explicit VisitorModuleBlackBox(bool only_modules = false, const slang::SourceManager* sm = nullptr);
        void handle(const slang::syntax::ModuleDeclarationSyntax &node);
        void handle(const slang::syntax::ModuleHeaderSyntax &node);
        void handle(const slang::syntax::AnsiPortListSyntax &port);
        void handle(const slang::syntax::ImplicitAnsiPortSyntax &port);
        void handle(const slang::syntax::ParameterDeclarationSyntax &node);
        void handle(const slang::syntax::HierarchyInstantiationSyntax &node);

        void set_source_manager(const slang::SourceManager *new_sm);

		// std::string module_name;
        
		std::unique_ptr<std::unordered_map<std::string,std::unique_ptr<ModuleBlackBox>>> read_bb;
};