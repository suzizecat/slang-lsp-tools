#include "index_reference_visitor.hpp"
#include <spdlog/spdlog.h>
namespace diplomat::index
{
	bool ReferenceVisitor::_add_reference_from_stx(const slang::syntax::SyntaxNode& node,
	                                               const std::string_view& name)
	{
		spdlog::info("    Found reference for name {}", name);
		IndexRange node_loc(node,*_sm);
		IndexFile* parent_file = _index->get_file(node_loc.start.file);

		IndexScope* ref_scope = parent_file->lookup_scope_by_range(node_loc);
		if(! ref_scope)
			return false;

		IndexSymbol* main_symb = ref_scope->lookup_symbol(std::string(name));
		if(! main_symb)
			return false;

		main_symb->add_reference(node_loc);
		parent_file->add_reference(main_symb,node_loc);

		return true;

	}

	void ReferenceVisitor::handle(const slang::syntax::IdentifierNameSyntax& node)
	{

		if(! _add_reference_from_stx(node, node.identifier.rawText()))
			visitDefault(node);
	}
} // namespace diplomat::index
