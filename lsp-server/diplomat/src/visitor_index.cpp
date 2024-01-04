
#include "visitor_index.hpp"

#include "slang/ast/Compilation.h"
#include "slang/text/SourceManager.h"
#include "fmt/format.h"
#include <iostream>
#include "spdlog/spdlog.h"
namespace ast = slang::ast;
namespace syntax = slang::syntax;


namespace slsp
{

RefVisitor::RefVisitor(DiplomatIndex* index, const ast::Scope& scope, const slang::SourceManager* sm) : 
    syntax::SyntaxVisitor<RefVisitor>(), 
    _index(index),
    _sm(sm),
    _scope(scope),
    _ref_filepath(sm->getFullPath(scope.asSymbol().getSyntax()->sourceRange().start().buffer())),
    _in_instanciation(false)
{}



bool RefVisitor::_add_reference(const syntax::ConstTokenOrSyntax& node, const std::string_view lookup_name)
{
    return _add_reference(node,std::string(lookup_name));
}

bool RefVisitor::_add_reference(const syntax::ConstTokenOrSyntax& node, const std::string& lookup_name)
{    
    const ast::Symbol* root_symb = _scope.lookupName(lookup_name);
    if(root_symb != nullptr)
    {
        _index->add_reference_to(*root_symb,node,_ref_filepath);
        return true;
    }
    return false;
}

void RefVisitor::handle(const syntax::NamedPortConnectionSyntax &node)
{
    if(_in_instanciation)
    {
        _add_reference(syntax::ConstTokenOrSyntax(&node),std::string(node.name.rawText()));
    }
    else
    {
        visitDefault(node);
    }
}

void RefVisitor::handle(const syntax::NamedParamAssignmentSyntax &node)
{
    if(_in_instanciation)
    {
        _add_reference(syntax::ConstTokenOrSyntax(&node),std::string(node.name.rawText()));
    }
    else
    {
        visitDefault(node);
    }
}


void RefVisitor::handle(const syntax::IdentifierNameSyntax &node)
{
    if(! _add_reference(syntax::ConstTokenOrSyntax(&node), node.identifier.rawText()))
        visitDefault(node);
}


IndexVisitor::IndexVisitor(const slang::SourceManager* sm) :
    ast::ASTVisitor<IndexVisitor,true,false>(),
    _sm(sm),
    _index(new DiplomatIndex(sm))
{
    spdlog::info("Index visitor built");
}


/**
 * @brief Visitor handle for instances in the CST.
 * Actually call the Reference visitor to populate the internal database.
 * 
 * @param node Node that is visited.
 */
void IndexVisitor::handle(const slang::ast::InstanceSymbol &node)
{
    visitDefault(node);
    // Reinstanciate as we need to switch to current scope (node.body) anyway.
    RefVisitor ref(_index.get(), node.body, _sm);
    
    const syntax::SyntaxNode* stx_node = node.body.getSyntax();
    
    if(stx_node != nullptr)
    {
        stx_node->visit(ref);
    }

    stx_node = node.getSyntax();

    if(stx_node != nullptr)
    {
        ref.set_ref_context(_sm->getFullPath(stx_node->sourceRange().start().buffer()),true);
        stx_node->parent->visit(ref);
    }
}


void IndexVisitor::handle(const ast::ValueSymbol &node)
{
    _index->add_symbol(node);
}


}