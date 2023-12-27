#ifndef _H_INCLUDE_EXPLORER_VISITOR
#define _H_INCLUDE_EXPLORER_VISITOR
#include <nlohmann/json.hpp>
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/ValueSymbol.h>
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <string>
#include <string_view>

#include <vector>
#include <unordered_map>
#include <unordered_set>

typedef std::unordered_map<const slang::ast::Symbol*, std::vector<std::string>> ref_table_t;

class RefVisitor : public slang::syntax::SyntaxVisitor<RefVisitor>
{

        const slang::SourceManager* _sm ;
        const slang::ast::Scope& _scope;
        ref_table_t* _ref_storage;
        bool _add_reference(const slang::SourceRange& location, const std::string& lookup_name);
        bool _add_reference(const slang::syntax::SyntaxNode& node, const std::string& lookup_name);
        bool _add_reference(const slang::syntax::SyntaxNode& node, const std::string_view lookup_name);
    public:
        explicit RefVisitor(ref_table_t* ref_storage, const slang::ast::Scope& scope, const slang::SourceManager* sm);
        bool in_instanciation;
        void handle(const slang::syntax::IdentifierNameSyntax& node);
        void handle(const slang::syntax::NamedPortConnectionSyntax& node);
        void handle(const slang::syntax::NamedParamAssignmentSyntax& node);
};

class ExplorerVisitor : public slang::ast::ASTVisitor<ExplorerVisitor,true,true>
{
    nlohmann::json_pointer<std::string> _pointer;
    nlohmann::json _refs;

    ref_table_t _ref_storage;

    public : 
    //void handle(const slang::syntax::ModuleDeclarationSyntax& node);
    
    void handle(const slang::ast::InstanceSymbol& node);
    void handle(const slang::ast::ValueSymbol& node);
    // template<typename T>
    // void handle(const T& node); // Base function, specialized in C++
    
    inline const nlohmann::json& get_refs() const {return _refs;}; 

};




#endif //_H_INCLUDE_EXPLORER_VISITOR