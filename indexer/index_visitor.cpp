#include "index_visitor.hpp"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "fmt/format.h"
#include "index_elements.hpp"
#include "index_exceptions.hpp"
#include "index_scopetree_node.hpp"
#include "index_symbols.hpp"
#include "slang/ast/Symbol.h"
#include "slang/ast/symbols/CompilationUnitSymbols.h"
#include "slang/ast/symbols/PortSymbols.h"
#include "slang/syntax/AllSyntax.h"
#include <spdlog/spdlog.h>
#include <slang/ast/types/DeclaredType.h>
#include <slang/parsing/Token.h>
#include <string_view>

using namespace slang::ast;

namespace diplomat::index {
	
	void IndexVisitor::_open_scope(const std::string &name, bool is_virtual, std::shared_ptr<IndexScope> data)
	{
		
		if(_scope_stack.empty())
		{	
			// If the scope already has a root scope, it is required to
			// use it again as root, in case of invalidation.
			if(_index->have_root_scope())
				_scope_stack.push(_index->get_root_scope());
			else
				_scope_stack.push(_index->set_root_scope(name));
		}
		else
		{
			IndexScopeTreeNode* added;
			if(name.empty())
				added = _scope_stack.top()->add_anon_child(data, is_virtual);
			else 
				added = _scope_stack.top()->add_child(name, data, is_virtual);
			_scope_stack.push(added);
		}
	}

	void IndexVisitor::_open_scope(const std::string_view &name, bool is_virtual, std::shared_ptr<IndexScope> data)
	{
		_open_scope(std::string(name),is_virtual);
	}


	void IndexVisitor::_close_scope(const std::string_view& name)
	{
		const IndexScopeTreeNode * curr_scope = _current_scope();
		
		if (curr_scope == nullptr)
			throw std::logic_error(fmt::format("Attempting to close scope {} while no scope are open.",name));
		// else if(curr_scope->is_anonymous() && ! name.empty() && name != curr_scope->get_name())
		// 	throw std::logic_error(fmt::format("Attempting to close scope {} while current scope is anonymous ({})",name, _current_scope()->get_name()));
		else if(! name.empty() && name != curr_scope->get_name())
			throw std::logic_error(fmt::format("Attempting to close scope {} while current scope name is {}",name, _current_scope()->get_name()));
		else
			_scope_stack.pop();
	}

	void IndexVisitor::_add_symbols_from_name_syntax(const slang::syntax::NameSyntax* node)
	{
		using namespace slang::syntax;
		switch (node->kind)
		{
		case SyntaxKind::IdentifierName:
			{
				const IdentifierNameSyntax& stx = node->as<IdentifierNameSyntax>();
				// IndexSymbol* new_symb = _index->add_symbol(stx.identifier.rawText(),{stx.identifier.range(),*_sm});
				_index->add_symbol( _current_scope()->add_symbol(std::make_unique<IndexSymbol>(stx, *_sm)));
				
			}	
			break;
		
		case SyntaxKind::EmptyIdentifierName:
			// Do nothing
			break;
		default:
			spdlog::error("Symbol NameSyntax of kind {} is not handled just yet.", toString(node->kind));
			break;
		}
	}

	void IndexVisitor::_default_symbol_handle(const slang::ast::Symbol& node,  const slang::syntax::SyntaxNode* matching_syntax)
	{
		// For scope to be analized, it shall not be already valid.
		if(_current_scope()->is_valid())
			return;

		if(! node.isScope() && _current_scope() && ! node.name.empty())
		{
			
			const slang::syntax::SyntaxNode* stx = matching_syntax ? matching_syntax : node.getSyntax();
			if(stx)
			{
				// if(node.kind == SymbolKind::Instance)
				// {
				// 	stx = stx->as<slang::syntax::HierarchicalInstanceSyntax>().decl;
				// }
				
				// In generate blocks, some variable defined as parameters (iterator in for-generate)
				// Are re-emitted by slang for whatever reason, so they need to be filtered out.
				if(node.kind == slang::ast::SymbolKind::Parameter && _current_scope()->lookup_symbol(stx->getFirstToken().rawText()))
				{
					// The node is actually present in the parent scope
					spdlog::debug("Skipped redundant symbol with location {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
				}
				else

				{
					IndexSymbol* nsymb = _index->add_symbol(_current_scope()->add_symbol(std::make_unique<IndexSymbol>(*stx,*_sm)));
					nsymb->set_kind(slang::ast::toString(node.kind));
					spdlog::debug("Added symbol with location {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
				}
			}
			else
				spdlog::debug("Skipped symbol without def {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
		}
		
		// You cannot use `visitDefault` on a Symbol& as this is a no-op.
		// See https://github.com/MikePopoloski/slang/issues/1453
		//visitDefault(node);
	}

	IndexScopeTreeNode* IndexVisitor::_default_scope_handle(const slang::ast::Scope &node, const std::string_view& scope_name, const bool is_virtual )
	{
		using namespace slang;
		const Symbol& s = node.asSymbol();
		spdlog::debug("Handling of scope {} of kind {}",scope_name, slang::ast::toString(s.kind));
					
		std::string_view used_scope_name = scope_name;

		const slang::syntax::SyntaxNode* stx = s.getSyntax(); 
		if(stx)
		{
			IndexFile* containing_file = _index->add_file(_sm->getFileName(stx->sourceRange().start()));
			
			if(s.kind == slang::ast::SymbolKind::CompilationUnit)
			{
				containing_file->set_syntax_root(stx);
				for(const auto& member : node.members())
					member.visit(*this);
				return nullptr;
			}
			// else if(s.kind == slang::ast::SymbolKind::Subroutine)
			// {
			// 	const syntax::FunctionPrototypeSyntax* stxproto = stx->as<syntax::FunctionDeclarationSyntax>().prototype;
			// 	const ast::SubroutineSymbol& subrout = s.as<ast::SubroutineSymbol>();
			// 	const ast::MethodPrototypeSymbol* proto = subrout.getPrototype();
			// 	if(! proto)
			// 	{
			// 		spdlog::warn("Subroutine without prototype");
			// 		return;
			// 	}


			// }
			else
			{
				if(containing_file->get_syntax_root() == nullptr)
				{
					spdlog::debug("Add syntax root from within the design for file {}",containing_file->get_path().generic_string());
					containing_file->set_syntax_root(stx);
				}
				IndexRange scope_range = IndexRange(stx->sourceRange(),*_sm);
				// IndexScopeTreeNode* duplicate = _current_scope()->get_child_by_exact_range(scope_range);
				// if(duplicate) 
				// {
				// 	_current_scope()->add_child_alias(duplicate->get_name(),std::string(scope_name));
				// 	_open_scope(duplicate->get_name(),is_virtual);

				// 	used_scope_name = duplicate->get_name();
				// 	spdlog::info("    Opened scope {} instead of requested duplicate {}", used_scope_name, scope_name);
				// }
				// else
				// {
					_open_scope(scope_name,is_virtual);
					_current_scope()->set_source(IndexRange(stx->sourceRange(),*_sm));
					containing_file->register_scope(_current_scope());

					// #ifdef DIPLOMAT_DEBUG
					// _current_scope()->set_kind(slang::ast::toString(node.asSymbol().kind));
					// #endif
				// }

			}
		}
		else
		{
			_open_scope(scope_name);
		}

		IndexScopeTreeNode* ret = _current_scope();
		//_default_symbol_handle(s);
		if(! _current_scope()->is_valid())
		{
			for(const auto& member : node.members())
				member.visit(*this);

			_current_scope()->validate();
		}
		_close_scope(used_scope_name);

		return ret;
	}

	IndexScopeTreeNode* IndexVisitor::_default_scope_handle(const slang::ast::Scope& node, const bool is_virtual)
	{
		return _default_scope_handle(node,node.asSymbol().name,is_virtual);
	}

	void IndexVisitor::handle(const slang::ast::Scope& node)
	{
		bool is_virtual = node.asSymbol().kind != slang::ast::SymbolKind::InstanceBody;
		_default_scope_handle(node,is_virtual);
		
	}


	// void IndexVisitor::handle(const slang::ast::DefinitionSymbol& node)
	// {
	// 	_default_symbol_handle(node);
	// 	visitDefault(node);
	// }

	void IndexVisitor::handle(const slang::ast::VariableSymbol& node)
	{
		_default_symbol_handle(node);
		visitDefault(node);
	}

	void IndexVisitor::handle(const slang::ast::GenvarSymbol& node)
	{
		_default_symbol_handle(node);
		visitDefault(node);
	}
	
	void IndexVisitor::handle(const slang::ast::ParameterSymbol& node)
	{
		_default_symbol_handle(node);
		visitDefault(node);
	}

	void IndexVisitor::handle(const slang::ast::TransparentMemberSymbol& node)
	{
		_default_symbol_handle(node.wrapped);
		visitDefault(node);
	}


	void IndexVisitor::handle(const slang::ast::InstanceSymbol& node)
	{
		using namespace slang::syntax;
		
		// Visit the scope before the next code block in order to setup the scope.
		const slang::syntax::SyntaxNode* stx = node.getSyntax();
		if(stx)
		{
			stx = stx->as<slang::syntax::HierarchicalInstanceSyntax>().decl;
		}
		_default_symbol_handle(node, stx );
		
		// getCanonicalBody will return 0 on the canonical body itself
		// Therefore, it should have been cached previously if it is non-zero
		// And the scope should be cached otherwise.
		const uintptr_t ref_scope_key = reinterpret_cast<const uintptr_t>(node.getCanonicalBody());
		
		if(ref_scope_key)
		{
			IndexScopeTreeNode* cached_scope = _index->get_cached_scope(ref_scope_key);
			if(cached_scope)
			{
				spdlog::debug("Hit cached scope {} for {}",cached_scope->get_name(), node.name);
				_current_scope()->add_subtree_child(cached_scope, std::string{node.name});
				return ;
			}
			else
			{
				spdlog::error("Missed cache reference for scope {} while non-canon body.", node.name);
			}
		}
		//else
		// Do not 'else' in order to recover from a failed ref_scope_key
		{

			// Check if any interface are bound to the instance we are handling
			IndexScopeTreeNode* inst_scope= nullptr;

			for(const PortConnection* pconn : node.getPortConnections())
			{
				const auto itf_bind = pconn->getIfaceConn();
				if(itf_bind.first != nullptr)
				{
					if(! inst_scope)
					{
						_open_scope(node.name);
						inst_scope = _current_scope();
						_close_scope(node.name);
					}
					IndexScopeTreeNode* local_itf_scope = _current_scope()->get_scope_by_name(itf_bind.first->name);
					if(local_itf_scope)
						inst_scope->add_subtree_child(local_itf_scope,std::string(pconn->port.name));

					
				}
			}

			IndexScopeTreeNode* new_scope = _default_scope_handle(node.body,node.name,false);
			if(! new_scope)
				throw index_exception("Instance symbol scope handling returned a nullptr scope.");

			spdlog::debug("Add scope to cache {}", new_scope->get_name());
			_index->cache_scope(reinterpret_cast<uintptr_t>(&(node.body)), new_scope);

	

			// When running into an instance, add the declared type to the scope of the instance.
			// This allows adding the module name to a scope related to its source file easily.
			const SyntaxNode* mod = node.body.getSyntax();
			if(mod)
			{
				// Using get_scope_by_name will resolve any duplicated scope.
				IndexScopeTreeNode* module_scope = _current_scope()->get_scope_by_name(node.name);
				// Manual insertion of the module name as a symbol to the target scope...
				// _open_scope(module_scope->get_name(),false);
				const slang::parsing::Token inst_typename = mod->as<ModuleDeclarationSyntax>().header->name; 
				//IndexSymbol* new_symb = _index->add_symbol(inst_typename.rawText(),{inst_typename.range(),*_sm},"<Module>");
				new_scope->add_symbol(std::make_unique<IndexSymbol>(inst_typename.rawText(),IndexRange{inst_typename.range(),*_sm}));

				spdlog::debug("Added symbol with location {}.{} of kind <Module>",new_scope->get_full_path(),inst_typename.rawText());
				
			}
		}


	}

	// void IndexVisitor::handle(const slang::ast::InterfacePortSymbol& node)
	// {
	// 	using namespace slang;
	// 	// Interface port symbol require to create a virtual sub-scope representing the content of the interface...
	// 	// The scope name will be symbol name (and the symbol is also on the symbol name...? )
	// 	spdlog::info("Processing interface");

	// 	// Record the node itself as a symbol, in particular for renaming purposes.
	// 	_default_symbol_handle(node);

	// 	const ast::DefinitionSymbol* defsymb = node.interfaceDef;
	// 	_open_scope(node.name);

	// }

	void IndexVisitor::handle(const slang::ast::SubroutineSymbol& node)
	{
		using namespace slang;
		_default_scope_handle(node,node.name,true); // Virtual to be checked...


		const syntax::SyntaxNode* stx = node.getSyntax();
		if(stx && stx->kind == syntax::SyntaxKind::FunctionDeclaration)
		{
			// // Using get_scope_by_name will resolve any duplicated scope.
			// IndexScope* module_scope = _current_scope()->get_scope_by_name(node.name);

			// // Manual insertion of the module name as a symbol to the target scope...
			// _open_scope(module_scope->get_name(),false);

			const syntax::FunctionPrototypeSyntax* stxproto = stx->as<syntax::FunctionDeclarationSyntax>().prototype;
			_add_symbols_from_name_syntax(stxproto->name);

			// _close_scope(module_scope->get_name());

		}


	}

	void IndexVisitor::handle(const slang::ast::WildcardImportSymbol& node)
	{

		// const slang::syntax::SyntaxNode* stx = node.getSyntax();
		// if(stx)
		// {
		// 	IndexRange import_source = IndexRange(stx->sourceRange(),*_sm);
		// 	IndexFile* containing_file = _index->add_file(import_source.start.file);


		// 	//containing_file->record_additionnal_lookup_scope(std::string(node.packageName));
		// }

		// visitDefault(node);
	}

} // namespace diplomat::index
