#pragma once
#include "slang/ast/Scope.h"
#include <slang/syntax/AllSyntax.h>
#include <slang/ast/ASTVisitor.h>

#include <string>
#include <string_view>
#include <stack>
#include <unordered_map>

#include "index_core.hpp"
#include "index_scopetree_node.hpp"

namespace diplomat::index {
	
	// Visit statements and bad but not expressions
	/**
	 * @brief This class will build the Scope Tree and store the found symbols
	 * 
	 */
	class IndexVisitor : public slang::ast::ASTVisitor<IndexVisitor,true,true,true, false>
	{
		protected :
			const slang::SourceManager* _sm;
			std::unique_ptr<IndexCore> _index;

			std::stack<IndexScopeTreeNode *> _scope_stack;

			/**
			 * @brief Holds the duplicate scopes as managed by slang.
			 * Mainly used in order to manage the cache mechanism on InstanceBodySymbols.
			 * If a  scope is registered here, it means that it already has a scope 
			 * (that does not requires a new processing)
			 */
			std::unordered_map<slang::ast::Scope*, const IndexScopeTreeNode* > _duplicate_scopes_map;


			void _open_scope(const std::string& name, bool is_virtual = false, std::shared_ptr<IndexScope> data = nullptr);
			void _open_scope(const std::string_view& name, bool is_virtual = false,  std::shared_ptr<IndexScope> data = nullptr);
			
			/**
			 * @brief This function closes the current scope, removing it from the
			 *  scope stack.
			 * 
			 * @note The name is required as a parameter to enforce consistency across open/close
			 * 
			 * @param name Name of the scope to close. It mus be equivalent to _scope_stack.top() name
			 */
			//void _close_scope(const std::string& name);
			void _close_scope(const std::string_view& name);

			/**
			 * @brief This function will take care of managing all "NameSyntax" derivatives from which
			 * we may want create one (or multiple) symbols
			 * 
			 * @param node Node to use as a symbol token
			 */
			void _add_symbols_from_name_syntax(const slang::syntax::NameSyntax* node);

			void _default_symbol_handle(const slang::ast::Symbol& node);
			
			/**
			 * @brief This function assumes the standard low-level scope handling.
			 * 
			 * @note As a limitation of {@link visitDefault}, the call to _default_symbol_ll should always be followed by
			 * a call to visit default.
			 *
			 * @param node Node representing the scope
			 * @param scope_name actual name used for the scope for {@link IndexScope} lookups.
			 * @param is_virtual true if the scope is virtual (elements from inside have access to the parent scope)
			 * @returns const IndexScopeTreeNode* a pointer to the directly created scope (if any) or nullptr otherwise
			 */
			IndexScopeTreeNode* _default_scope_handle(const slang::ast::Scope& node, const std::string_view& scope_name, const bool is_virtual = false);
			IndexScopeTreeNode* _default_scope_handle(const slang::ast::Scope& node, const bool is_virtual = false);
			inline IndexScopeTreeNode* _current_scope() const {return _scope_stack.empty() ? nullptr : _scope_stack.top(); };
		public: 
			explicit IndexVisitor(const slang::SourceManager* sm) : _sm(sm), _index(new IndexCore()) {};

			//inline const IndexCore* get_index() const {return _index.get(); };

			void handle(const slang::ast::Scope& node);
			// void handle(const slang::ast::Symbol& node);
			void handle(const slang::ast::DefinitionSymbol& node);
			//void handle(const slang::ast::PortSymbol& node);
			void handle(const slang::ast::VariableSymbol& node);
			void handle(const slang::ast::GenvarSymbol& node);
			void handle(const slang::ast::ParameterSymbol& node);
			void handle(const slang::ast::TransparentMemberSymbol& node);
			void handle(const slang::ast::InstanceSymbol& node);
			void handle(const slang::ast::SubroutineSymbol& node);
			void handle(const slang::ast::WildcardImportSymbol& node);

			inline std::unique_ptr<IndexCore> get_index() {return std::move(_index);};


	};
}
