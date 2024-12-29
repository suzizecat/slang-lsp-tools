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

	void IndexVisitor::_default_symbol_handle(const slang::ast::Symbol &node)
	{
		if(! node.isScope() && _current_scope() && ! node.name.empty())
		{
			//IndexSymbol* new_symb = _current_scope()->add_symbol(std::string(node.name));
			const slang::syntax::SyntaxNode* stx = node.getSyntax();
			if(stx)
			{

				IndexSymbol* new_symb = _index->add_symbol(node.name,{stx->sourceRange(),*_sm}, slang::ast::toString(node.kind));
				_current_scope()->add_symbol(new_symb);
				spdlog::info("Added symbol with location {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
			}
			else
				spdlog::info("Skipped symbol without def {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
		}
		
		visitDefault(node);
	}

	void IndexVisitor::_default_scope_handle(const slang::ast::Scope &node, const std::string_view& scope_name, const bool is_virtual )
	{
		const Symbol& s = node.asSymbol();
		spdlog::info("Handling of scope {} of kind {}",scope_name, slang::ast::toString(s.kind));
		
		//_open_scope(scope_name, is_virtual);
			
		std::string_view used_scope_name = scope_name;

		//const auto* inst = node.getContainingInstance();
		const slang::syntax::SyntaxNode* stx = s.getSyntax(); //inst ? inst->getSyntax() : nullptr;
		if(stx)
		{
			IndexFile* containing_file = _index->add_file(_sm->getFileName(stx->sourceRange().start()));
			
			if(s.kind == slang::ast::SymbolKind::CompilationUnit)
			{
				containing_file->set_syntax_root(stx);
				// No need to go through the compilation unit anyway.
				return;
			}
			else
			{
				IndexRange scope_range = IndexRange(stx->sourceRange(),*_sm);
				IndexScope* duplicate = _current_scope()->get_child_by_exact_range(scope_range);

				if(duplicate) 
				{
					_open_scope(duplicate->get_name());
					used_scope_name = duplicate->get_name();
					spdlog::info("Opened scope {} instead of requested duplicate {}", used_scope_name, scope_name);
				}
				else
				{
					_open_scope(scope_name);
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

	// void IndexVisitor::handle(const slang::ast::PortSymbol& node)
	// {
	// 	_default_symbol_handle(node);
	// }

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

	void IndexVisitor::handle(const slang::ast::InstanceSymbol& node)
	{
		using namespace slang::syntax;

		// When running into an instance, add the declared type to the scope of the instance.
		// This allows adding the module name to a scope related to its source file easily.
		const SyntaxNode* mod = node.body.getSyntax();
		if(mod)
		{
			_open_scope(node.name,false);
			const slang::parsing::Token inst_typename = mod->as<ModuleDeclarationSyntax>().header->name; 
			IndexSymbol* new_symb = _index->add_symbol(inst_typename.rawText(),{inst_typename.range(),*_sm},"<Module>");
			_current_scope()->add_symbol(new_symb);

			spdlog::info("Added symbol with location {}.{} of kind <Module>",_current_scope()->get_full_path(),inst_typename.rawText());
			
			_close_scope(node.name);
		}
		_default_symbol_handle(node);
		_default_scope_handle(node.body,node.name,false);

		//visitDefault(node);
	}
	
} // namespace diplomat::index