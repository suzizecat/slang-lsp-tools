#ifndef _H_INCLUDE_EXPLORER_VISITOR
#define _H_INCLUDE_EXPLORER_VISITOR
#include <slang/ast/symbols/InstanceSymbols.h>
#include <slang/ast/symbols/ValueSymbol.h>
#include <slang/ast/ASTVisitor.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/parsing/Token.h>
#include <string>
#include <string_view>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <filesystem>

#include "diplomat_index.hpp"



namespace slsp
{
    
/// Use this type as a base class for syntax tree visitors. It will default to
/// traversing all children of each node. Add implementations for any specific
/// node types you want to handle.
template<typename TDerived>
class SyntaxSelectVisitor {
protected:

    #ifdef DEBUG
        std::vector<slang::syntax::SyntaxKind> _kind_tree;
    #endif

public:
    /// Visit the provided node, of static type T.
    template<typename T>
    void visit(T&& t) {
        #ifdef DEBUG
            _kind_tree.push_back(t.kind);
        #endif
        if constexpr (requires { static_cast<TDerived*>(this)->handle(t); })
            static_cast<TDerived*>(this)->handle(t);
        else
            static_cast<TDerived*>(this)->visitDefault(t);

         #ifdef DEBUG
            _kind_tree.pop_back();
        #endif   
    }

    /// The default handler invoked when no visit() method is overridden for a particular type.
    /// Will visit all child nodes by default.
    template<typename T>
    void visitDefault(T&& node) {
        if(_to_skip && _to_skip->contains(&node))
            return;
        for (uint32_t i = 0; i < node.getChildCount(); i++) {
            auto child = node.childNode(i);
            if (child)
                child->visit(*static_cast<TDerived*>(this));
            else {
                auto token = node.childToken(i);
                if (token)
                    static_cast<TDerived*>(this)->visitToken(token);
            }
        }
    }

protected:
    const std::unordered_set<const slang::syntax::SyntaxNode*>* _to_skip;
    


private:
    // This is to make things compile if the derived class doesn't provide an implementation.
    void visitToken(slang::parsing::Token) {}
};


    typedef std::unordered_map<const slang::ast::Symbol*, std::vector<std::string>> ref_table_t;


struct ScopeContext {
                std::vector<std::string> _scoped_name;
                const slang::syntax::ScopedNameSyntax* _scoped_name_root;
                ScopeContext() : _scoped_name(), _scoped_name_root(nullptr) {};
            };

        class ScopeGuard {
            std::unique_ptr<ScopeContext> _previous_context;
            std::unique_ptr<ScopeContext>* _original_ref;

            public:
                explicit ScopeGuard(std::unique_ptr<ScopeContext>* swapped_context)
                {
                    _original_ref = swapped_context;
                    _previous_context = std::move(*swapped_context);
                    *swapped_context = std::make_unique<ScopeContext>();
                }

                ~ScopeGuard()
                {
                    std::swap(_previous_context,*_original_ref);
                }
        };

    class RefVisitor : public SyntaxSelectVisitor<RefVisitor>
    {
            DiplomatIndex* _index;

            
            const slang::SourceManager* _sm ;
            const slang::ast::Scope& _scope;

            std::optional<const slang::ast::Symbol*> _instance_type_symbol;
            std::string_view _instance_type_name;

            std::filesystem::path _ref_filepath;

            std::unique_ptr<ScopeContext> _name_context; 

            //bool _add_reference(const slang::SourceRange& location, const std::string& lookup_name);
            bool _add_reference(const slang::syntax::ConstTokenOrSyntax& node, const std::string& lookup_name);
            bool _add_reference(const slang::syntax::ConstTokenOrSyntax& node, const std::string_view lookup_name);
            std::string _get_scoped_name_prefix();

            bool _in_instanciation;
        
        public:
            explicit RefVisitor(DiplomatIndex* index, 
                const slang::ast::Scope& scope, 
                const slang::SourceManager* sm, 
                const std::unordered_set<const slang::syntax::SyntaxNode*>* _explored = nullptr);

            void handle(const slang::syntax::ModuleHeaderSyntax& node);
            void handle(const slang::syntax::HierarchyInstantiationSyntax& node);
            void handle(const slang::syntax::IdentifierNameSyntax& node);
            void handle(const slang::syntax::IdentifierSelectNameSyntax& node);
            void handle(const slang::syntax::ElementSelectSyntax& node);
            void handle(const slang::syntax::NamedPortConnectionSyntax& node);
            void handle(const slang::syntax::NamedParamAssignmentSyntax& node);
            void handle(const slang::syntax::DeclaratorSyntax& node);
            void handle(const slang::syntax::ClassMethodDeclarationSyntax& node);
            void handle(const slang::syntax::ScopedNameSyntax& node);
            void handle(const slang::syntax::MacroUsageSyntax& node);

            
            inline void set_ref_context(const std::filesystem::path& path, const bool in_instanciation) 
            {
                _ref_filepath = path;
                _in_instanciation = in_instanciation;
            };

            inline bool is_valid()
            {
                return ! _ref_filepath.empty();
            }

            inline bool process_macro(const slang::syntax::SyntaxNode& node)
            {
                const slang::syntax::SyntaxNode* first_subnode = node.childNode(0);
                if(! first_subnode || first_subnode->kind != slang::syntax::SyntaxKind::MacroUsage)
                    return false;
                else
                {
                    visit(first_subnode->as<slang::syntax::MacroUsageSyntax>());
                    return true;
                }
            }
    };

    class IndexVisitor : public slang::ast::ASTVisitor<IndexVisitor,true,false>
    {
        protected:
            const slang::SourceManager* _sm ;
            std::unique_ptr<DiplomatIndex> _index;
            std::deque<const slang::syntax::SyntaxNode*> _to_explore;
            std::unordered_set<const slang::syntax::SyntaxNode*> _explored;
        public : 
            //void handle(const slang::syntax::ModuleDeclarationSyntax& node);
            explicit IndexVisitor(const slang::SourceManager* sm);

            void handle(const slang::ast::InstanceSymbol& node);
            void handle(const slang::ast::ValueSymbol& node);
            void handle(const slang::ast::ClassType& node);
            //void handle(const slang::ast::ClassPropertySymbol& node);
            void handle(const slang::ast::SubroutineSymbol& node);
            void handle(const slang::ast::FormalArgumentSymbol& node);

            void handle(const slang::ast::Scope& node);

            inline DiplomatIndex* get_index() {return _index.get(); };
            inline std::unique_ptr<DiplomatIndex> extract_index() {return std::move(_index); };
    };

} // namespace slsp


#endif //_H_INCLUDE_EXPLORER_VISITOR