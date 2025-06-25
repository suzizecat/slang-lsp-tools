#pragma once
#include "index_core.hpp"
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/ast/ASTVisitor.h>

#include <string>
#include <string_view>
#include <stack>
namespace diplomat::index {
	// Visit statements and bad but not expressions
	class IndexVisitor : public slang::ast::ASTVisitor<IndexVisitor,true,true,true>
	{
		protected :
			const slang::SourceManager* _sm;
			std::unique_ptr<IndexCore> _index;

			std::stack<IndexScope *> _scope_stack;


			void _open_scope(const std::string& name, bool is_virtual = false);
			void _open_scope(const std::string_view& name, bool is_virtual = false);
			
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
			void _default_scope_handle(const slang::ast::Scope& node, const std::string_view& scope_name, const bool is_virtual = false);
			void _default_scope_handle(const slang::ast::Scope& node, const bool is_virtual = false);
			inline IndexScope* _current_scope() const {return _scope_stack.empty() ? nullptr : _scope_stack.top(); };
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