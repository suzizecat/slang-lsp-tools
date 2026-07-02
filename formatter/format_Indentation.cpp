#include "format_Indentation.hpp"
#include "spdlog/spdlog.h"
using namespace slang::syntax;

IndentationPhaseVisitor::IndentationPhaseVisitor(SpacingManager* idt) :
_mem(),
_idt(idt),
_has_done_work(false)
{
}


void IndentationPhaseVisitor::handle(const slang::syntax::ProceduralBlockSyntax& node)
{
	if(_default_indent_handler(node))
	{
		spdlog::info("Indented block of kind {}, skip processing", toString(node.kind));
	}
	else 
	{
		spdlog::info("Increase indent due to kind {}",toString(node.kind));
		IndentLock idt(*_idt);
		visitDefault(node);
	}
	
}


bool IndentationPhaseVisitor::_default_indent_handler(const slang::syntax::SyntaxNode& node)
{
	Token first_token = node.getFirstToken();

	if(! first_token.isOnSameLine() && ! _idt->check_indent(first_token))
	{
		_has_done_work = true;
		SyntaxNode* stx = deepClone(node,_mem) ;
		*(stx->getFirstTokenPtr()) = _idt->indent(first_token);
		replace(node,*stx);
		return true;
	}

	return false;
}




void IndentationPhaseVisitor::handle(const slang::syntax::ModuleDeclarationSyntax& node)
{

	{
		IndentLock idt(*_idt);
		spdlog::info("Increase indent due to kind {}",toString(node.kind));
		visitDefault(node.members);
	}
		
}


void IndentationPhaseVisitor::handle(const slang::syntax::ConditionalStatementSyntax& node)
{
	{
		spdlog::info("Increase indent due to kind {}",toString(node.kind));
		IndentLock idt(*_idt);
		visitDefault(node);
	}
	//_default_indent_handler(node);
}


void IndentationPhaseVisitor::handle(const slang::syntax::DataDeclarationSyntax& node)
{
	_default_indent_handler(node);
}

// void IndentationPhaseVisitor::handle(const slang::syntax::ProceduralBlockSyntax& node)
// {
// 	{
// 		spdlog::info("Increase indent due to kind {}",toString(node.kind));
// 		IndentLock idt(*_idt);
//         visitDefault(node);
//     }
//     _default_indent_handler(node);
// }

void IndentationPhaseVisitor::handle(const slang::syntax::MemberSyntax& node)
{
	_default_indent_handler(node);
}

void IndentationPhaseVisitor::handle(const slang::syntax::ExpressionSyntax& node)
{
	_default_indent_handler(node);
}
