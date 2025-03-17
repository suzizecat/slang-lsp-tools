#include "format_StatementExpansion.hpp"

using namespace slang::syntax;

StatementExpansionPhaseVisitor::StatementExpansionPhaseVisitor(SpacingManager* idt) :
_mem(),
_idt(idt)
{
}

void StatementExpansionPhaseVisitor::handle(const slang::syntax::DataDeclarationSyntax& node)
{

	if(node.declarators.size() == 1)
		return;
	
	slang::SmallVector<TokenOrSyntax> subvector;
	for(int i = 0; i < node.declarators.size(); i ++)
	{	
		DataDeclarationSyntax* new_elt = deepClone(node,_mem);
		subvector.push_back(new_elt->declarators[i]);
		new_elt->declarators = subvector.copy(_mem);
		if(i > 0)
		{
			Token* first_tok = new_elt->getFirstTokenPtr();
			*first_tok = _idt->remove_extra_newlines(*first_tok,true);
		}
		insertAfter(node,*new_elt);
		subvector.clear();
	}

	remove(node);
}
