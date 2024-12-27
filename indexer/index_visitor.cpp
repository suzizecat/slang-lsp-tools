#include "index_visitor.hpp"
#include <stdexcept>
#include "fmt/format.h"
#include <spdlog/spdlog.h>

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
		else if(name != curr_scope->get_name())
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
				IndexSymbol* new_symb = _index->add_symbol(node.name,{stx->sourceRange(),*_sm});
				_current_scope()->add_symbol(new_symb);
				spdlog::info("Added symbol with location {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
			}
			else
				spdlog::info("Skiped symbol without def  {}.{} of kind {}",_current_scope()->get_full_path(),node.name,slang::ast::toString(node.kind));
		}
		
		visitDefault(node);
	}

	void IndexVisitor::_default_scope_handle(const slang::ast::Scope &node, const bool is_virtual )
	{
		const Symbol& s = node.asSymbol();
		spdlog::info("Handling of scope {} of kind {}",s.name, slang::ast::toString(s.kind));
		// if(! s.name.empty())
		// {
			_open_scope(s.name, is_virtual);

			const auto* inst = node.getContainingInstance();
			const slang::syntax::SyntaxNode* stx = s.getSyntax(); //inst ? inst->getSyntax() : nullptr;
			if(stx)
			{
				_index->get_file(_sm->getFileName(stx->sourceRange().start()))->register_scope(_current_scope());
				_current_scope()->set_source(IndexRange(stx->sourceRange(),*_sm));
			}

			visitDefault(node);
			_close_scope(s.name);
	}

	void IndexVisitor::handle(const slang::ast::Scope& node)
	{
		_default_scope_handle(node);
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
}