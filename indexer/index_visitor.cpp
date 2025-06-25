#include "index_visitor.hpp"
#include <stdexcept>
#include "fmt/format.h"
#include <spdlog/spdlog.h>
#include <slang/ast/types/DeclaredType.h>
#include <slang/parsing/Token.h>

using namespace slang::ast;

namespace diplomat::index {
	
	void IndexVisitor::_open_scope(const std::string &name, bool is_virtual)
	{
		
		if(_scope_stack.empty())
		{
			_scope_stack.push(_index->set_root_scope(name));
		}
		else
		{
			_scope_stack.push(_scope_stack.top()->add_child(name, is_virtual));
		}
	}

	void IndexVisitor::_open_scope(const std::string_view &name, bool is_virtual)
	{
		_open_scope(std::string(name),is_virtual);
	}


	void IndexVisitor::_close_scope(const std::string_view& name)
	{
		const IndexScope * curr_scope = _current_scope();
		
		if (curr_scope == nullptr)
			throw std::logic_error(fmt::format("Attempting to close scope {} while no scope are open.",name));
		else if(curr_scope->is_anonymous() && ! name.empty() && name != curr_scope->get_name())
			throw std::logic_error(fmt::format("Attempting to close scope {} while current scope is anonymous ({})",name, _current_scope()->get_name()));
		else if(! curr_scope->is_anonymous() && name != curr_scope->get_name())
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
				IndexSymbol* new_symb = _index->add_symbol(stx.identifier.rawText(),{stx.identifier.range(),*_sm});
				_current_scope()->add_symbol(new_symb);
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

	void IndexVisitor::_default_symbol_handle(const slang::ast::Symbol& node)
	{
		if(! node.isScope() && _current_scope() && ! node.name.empty())
		{
			
			const slang::syntax::SyntaxNode* stx = node.getSyntax();
			if(stx)
			{

				IndexSymbol* new_symb = _index->add_symbol(node.name,{stx->sourceRange(),*_sm}, slang::ast::toString(node.kind));
				_current_scope()->add_symbol(new_symb);
				spdlog::debug("Added symbol with location {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
			}
			else
				spdlog::debug("Skipped symbol without def {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
		}
		
		visitDefault(node);
	}

	void IndexVisitor::_default_scope_handle(const slang::ast::Scope &node, const std::string_view& scope_name, const bool is_virtual )
	{
		using namespace slang;
		const Symbol& s = node.asSymbol();
		spdlog::info("Handling of scope {} of kind {}",scope_name, slang::ast::toString(s.kind));
					
		std::string_view used_scope_name = scope_name;

		const slang::syntax::SyntaxNode* stx = s.getSyntax(); 
		if(stx)
		{
			IndexFile* containing_file = _index->add_file(_sm->getFileName(stx->sourceRange().start()));
			
			if(s.kind == slang::ast::SymbolKind::CompilationUnit)
			{
				containing_file->set_syntax_root(stx);
				visitDefault(node);
				return;
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
				IndexRange scope_range = IndexRange(stx->sourceRange(),*_sm);
				IndexScope* duplicate = _current_scope()->get_child_by_exact_range(scope_range);
				if(duplicate) 
				{
					_current_scope()->add_child_alias(duplicate->get_name(),std::string(scope_name));
					_open_scope(duplicate->get_name(),is_virtual);

					used_scope_name = duplicate->get_name();
					spdlog::info("    Opened scope {} instead of requested duplicate {}", used_scope_name, scope_name);
				}
				else
				{
					_open_scope(scope_name,is_virtual);
					_current_scope()->set_source(IndexRange(stx->sourceRange(),*_sm));
					
					containing_file->register_scope(_current_scope());

					#ifdef DIPLOMAT_DEBUG
					_current_scope()->set_kind(slang::ast::toString(node.asSymbol().kind));
					#endif
				}

			}
		}
		else
		{
			_open_scope(scope_name);
		}

		//_default_symbol_handle(s);
		visitDefault(node);
		_close_scope(used_scope_name);
	}

	void IndexVisitor::_default_scope_handle(const slang::ast::Scope& node, const bool is_virtual)
	{
		_default_scope_handle(node,node.asSymbol().name,is_virtual);
	}

	void IndexVisitor::handle(const slang::ast::Scope& node)
	{
		bool is_virtual = node.asSymbol().kind != slang::ast::SymbolKind::InstanceBody;
		_default_scope_handle(node,is_virtual);
	}


	void IndexVisitor::handle(const slang::ast::DefinitionSymbol& node)
	{
		_default_symbol_handle(node);
	}

	void IndexVisitor::handle(const slang::ast::VariableSymbol& node)
	{
		_default_symbol_handle(node);
	}

	void IndexVisitor::handle(const slang::ast::GenvarSymbol& node)
	{
		_default_symbol_handle(node);
	}
	
	void IndexVisitor::handle(const slang::ast::ParameterSymbol& node)
	{
		_default_symbol_handle(node);
	}

	void IndexVisitor::handle(const slang::ast::TransparentMemberSymbol& node)
	{
		_default_symbol_handle(node.wrapped);
	}


	void IndexVisitor::handle(const slang::ast::InstanceSymbol& node)
	{
		using namespace slang::syntax;

		const SyntaxNode* mod = node.body.getSyntax();
		
		// Visit the scope before the next code block in order to setup the scope.
		_default_symbol_handle(node);
		_default_scope_handle(node.body,node.name,false);

		// When running into an instance, add the declared type to the scope of the instance.
		// This allows adding the module name to a scope related to its source file easily.
		if(mod)
		{
			// Using get_scope_by_name will resolve any duplicated scope.
			IndexScope* module_scope = _current_scope()->get_scope_by_name(node.name);

			// Manual insertion of the module name as a symbol to the target scope...
			_open_scope(module_scope->get_name(),false);
			const slang::parsing::Token inst_typename = mod->as<ModuleDeclarationSyntax>().header->name; 
			IndexSymbol* new_symb = _index->add_symbol(inst_typename.rawText(),{inst_typename.range(),*_sm},"<Module>");
			_current_scope()->add_symbol(new_symb);

			spdlog::info("Added symbol with location {}.{} of kind <Module>",_current_scope()->get_full_path(),inst_typename.rawText());
			
			_close_scope(module_scope->get_name());
		}


	}

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

		const slang::syntax::SyntaxNode* stx = node.getSyntax();
		if(stx)
		{
			IndexRange import_source = IndexRange(stx->sourceRange(),*_sm);
			IndexFile* containing_file = _index->add_file(import_source.start.file);


			containing_file->record_additionnal_lookup_scope(std::string(node.packageName));
		}

		visitDefault(node);
	}

} // namespace diplomat::index