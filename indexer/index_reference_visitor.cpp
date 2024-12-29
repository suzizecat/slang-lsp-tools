#include "index_reference_visitor.hpp"
#include <spdlog/spdlog.h>
namespace diplomat::index
{
	bool ReferenceVisitor::_add_reference_from_stx(const slang::SourceRange & loc,
	                                               const std::string_view& name)
	{
		spdlog::info("    Found reference for name {}", name);
		IndexRange node_loc(loc,*_sm);
		IndexFile* parent_file = _index->add_file(node_loc.start.file);

		IndexScope* ref_scope = parent_file->lookup_scope_by_range(node_loc);
		if(! ref_scope)
		{
			spdlog::debug("        Reference dropped: missing scope");
			return false;
		}
		IndexSymbol* main_symb = ref_scope->lookup_symbol(std::string(name));
		if(! main_symb)
		{
			spdlog::debug("        Reference dropped: failed to find symbol {} from scope {}",name, ref_scope->get_full_path());
			return false;
		}

		main_symb->add_reference(node_loc);
		parent_file->add_reference(main_symb,node_loc);

		return true;

	}

	void ReferenceVisitor::_select_instance_scope(const IndexLocation& curr_scope_loc,
	                                              const ::std::string_view& next_scope)
	{
		IndexScope* curr_scope = _index->get_scope_by_position(curr_scope_loc);
		if(! curr_scope)
			_instance_scope = nullptr;

		IndexScope* new_scope = curr_scope->get_scope_by_name(next_scope);

		_instance_scope = new_scope ? new_scope : curr_scope; 
		
	}

	bool ReferenceVisitor::_add_reference_to_symbol(const slang::SourceRange& loc,
	                                                const std::string_view& symbol_name)
	{
		if(! _instance_scope)
			return false;

		IndexSymbol* main_symb = _instance_scope->lookup_symbol(std::string(symbol_name));
		if(! main_symb)
			return false;

		IndexRange node_loc(loc,*_sm);
		// This is most probably a cross-reference.
		// Hence, the reference is situated at @loc while the symbol is elsewhere.
		IndexFile* ref_file = _index->get_file(node_loc.start.file);
		// IndexFile* parent_file = _index->get_file(main_symb->get_source() ? main_symb->get_source()->start.file : "UNDEF");
		// if(! parent_file)
		// 	return false;

		main_symb->add_reference(node_loc);
		ref_file->add_reference(main_symb,node_loc);

		return true;

		
	}

	void ReferenceVisitor::handle(const slang::syntax::HierarchicalInstanceSyntax& node)
	{
		if(! node.decl)
		{
			_instance_scope = nullptr;
		}
		else
		{
			_select_instance_scope(IndexLocation(node.sourceRange().start(),*_sm),node.decl->name.rawText());
			
			const slang::syntax::HierarchyInstantiationSyntax& root_instantiation_stx = node.parent->as<slang::syntax::HierarchyInstantiationSyntax>();
			_add_reference_to_symbol(node.decl->name.range(), root_instantiation_stx.type.rawText());
			
			// Manually visit the parameters in order to associate the parameter reference to the 
			// proper definition in the instance scope. 
			// Could be avoided multiple times, but most of the time, no loss.
			if(root_instantiation_stx.parameters)
				visitDefault(*(root_instantiation_stx.parameters));
			
			visitDefault(node);
			_instance_scope = nullptr;
		}
	}

	void ReferenceVisitor::handle(const slang::syntax::NamedPortConnectionSyntax& node)
	{
		_add_reference_to_symbol(node.name.range(),node.name.rawText());
		visitDefault(node);
	}

	void ReferenceVisitor::handle(const slang::syntax::NamedParamAssignmentSyntax& node)
	{
		_add_reference_to_symbol(node.name.range(),node.name.rawText());
		visitDefault(node);
	}

	void ReferenceVisitor::handle(const slang::syntax::HierarchyInstantiationSyntax& node)
	{
		// Manually skip the parameters visit, as this will be performed by the 
		// HierarchicalInstanceSyntax handler once we selected the underlying scope.
		visitDefault(node.instances);
	}

	void ReferenceVisitor::handle(const slang::syntax::IdentifierNameSyntax& node)
	{

		if(! _add_reference_from_stx(node.sourceRange(), node.identifier.rawText()))
			visitDefault(node);
	}
	
	void ReferenceVisitor::handle(const slang::syntax::IdentifierSelectNameSyntax& node)
	{
		_add_reference_from_stx(node.identifier.range(), node.identifier.rawText());
		// You may have IdentifierName within the SelectName syntax (within the bracket) so 
		// you must visit the node anyway.
		visitDefault(node);
	}
} // namespace diplomat::index
