#pragma once

#include <optional>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/syntax/AllSyntax.h>
#include <slang/parsing/Token.h>
#include <stack>
#include <memory>
#include <string_view>

#include "index_core.hpp"
#include "index_elements.hpp"
#include "index_scopetree_node.hpp"

namespace diplomat::index
{
	class IndexCore;

  class ReferenceVisitor : public slang::syntax::SyntaxVisitor<ReferenceVisitor>
  {
		const slang::SourceManager* _sm;
		IndexCore* _index;

		IndexScopeTreeNode* _instance_scope;

		/**
		 * @brief Lookup location used for evaluation of scoped expressions
		 * 
		 */
		std::optional<IndexLocation> _scoped_eval_lu_loc; 

		/**
		 * @brief Scope from which the scoped_eval has started.
		 * 
		 */
		IndexScopeTreeNode* _scoped_eval_root_scope;
		
		bool _add_reference_from_stx(const slang::SourceRange & loc, const std::string_view& name);
		bool _add_reference_to_symbol(const slang::SourceRange& loc, const std::string_view& symbol_name);

		/**
		 * @brief Set the internal _instance_scope for future use of _add_reference_to_symbol.
		 * 
		 * @param curr_scope_loc Any position used in the "parent" scope, 
		 * used for looking up the target scope as parent.<next_scope>. 
		 * @param next_scope Name of the scope to lookup.
		 * 
		 * @note Will set _instance_scope value. Set to nullptr if the scope is not found. 
		 */
		void _select_instance_scope(const IndexLocation& curr_scope_loc, const::std::string_view& next_scope);


	public :
		explicit ReferenceVisitor(const slang::SourceManager* sm, IndexCore* idx) : _sm(sm), _index(idx), _scoped_eval_lu_loc(), _scoped_eval_root_scope(nullptr), _instance_scope(nullptr) {};

			// void handle(const slang::syntax::ModuleHeaderSyntax& node);
			void handle(const slang::syntax::HierarchyInstantiationSyntax& node);
			
			void handle(const slang::syntax::IdentifierNameSyntax& node);
			void handle(const slang::syntax::IdentifierSelectNameSyntax& node);
			// void handle(const slang::syntax::ElementSelectSyntax& node);
			void handle(const slang::syntax::ModuleDeclarationSyntax& node);
			void handle(const slang::syntax::HierarchicalInstanceSyntax& node);
			void handle(const slang::syntax::NamedPortConnectionSyntax& node);
			void handle(const slang::syntax::NamedParamAssignmentSyntax& node);
			void handle(const slang::syntax::ScopedNameSyntax& node);

  };

}
