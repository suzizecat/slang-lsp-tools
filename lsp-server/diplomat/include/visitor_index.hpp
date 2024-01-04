#ifndef _H_INCLUDE_EXPLORER_VISITOR
#define _H_INCLUDE_EXPLORER_VISITOR
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/ValueSymbol.h>
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <string>
#include <string_view>

#include <vector>
#include <unordered_map>
#include <filesystem>

#include "diplomat_index.hpp"

namespace slsp
{
    


    typedef std::unordered_map<const slang::ast::Symbol*, std::vector<std::string>> ref_table_t;

    class RefVisitor : public slang::syntax::SyntaxVisitor<RefVisitor>
    {

            DiplomatIndex* _index;
            
            const slang::SourceManager* _sm ;
            const slang::ast::Scope& _scope;

            std::filesystem::path _ref_filepath;

            //bool _add_reference(const slang::SourceRange& location, const std::string& lookup_name);
            bool _add_reference(const slang::syntax::ConstTokenOrSyntax& node, const std::string& lookup_name);
            bool _add_reference(const slang::syntax::ConstTokenOrSyntax& node, const std::string_view lookup_name);
            bool _in_instanciation;
        public:
            explicit RefVisitor(DiplomatIndex* index, const slang::ast::Scope& scope, const slang::SourceManager* sm);
            void handle(const slang::syntax::IdentifierNameSyntax& node);
            void handle(const slang::syntax::NamedPortConnectionSyntax& node);
            void handle(const slang::syntax::NamedParamAssignmentSyntax& node);

            
            inline void set_ref_context(const std::filesystem::path& path, const bool in_instanciation) 
            {
                _ref_filepath = path;
                _in_instanciation = in_instanciation;
            };
    };

    class IndexVisitor : public slang::ast::ASTVisitor<IndexVisitor,true,false>
    {
        protected:
            std::unique_ptr<DiplomatIndex> _index;
            const slang::SourceManager* _sm ;
        public : 
            //void handle(const slang::syntax::ModuleDeclarationSyntax& node);
            explicit IndexVisitor(const slang::SourceManager* sm);

            void handle(const slang::ast::InstanceSymbol& node);
            void handle(const slang::ast::ValueSymbol& node);

            inline DiplomatIndex* get_index() {return _index.get(); };
    };

} // namespace slsp


#endif //_H_INCLUDE_EXPLORER_VISITOR