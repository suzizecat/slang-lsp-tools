
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

RefVisitor::RefVisitor(DiplomatIndex* index, 
const ast::Scope& scope, 
const slang::SourceManager* sm,
const std::unordered_set<const slang::syntax::SyntaxNode*>* _explored) : 
    SyntaxSelectVisitor<RefVisitor>(), 
    _index(index),
    _sm(sm),
    _scope(scope),
    _ref_filepath(sm->getFullPath(scope.asSymbol().getSyntax()->sourceRange().start().buffer())),
    _in_instanciation(false),
    _scoped_name_root(nullptr),
    _scoped_name()
{
    _to_skip = _explored;
}



bool RefVisitor::_add_reference(const syntax::ConstTokenOrSyntax& node, const std::string_view lookup_name)
{
    return _add_reference(node,std::string(lookup_name));
}

bool RefVisitor::_add_reference(const syntax::ConstTokenOrSyntax& node, const std::string& lookup_name)
{  
    const ast::Symbol* root_symb = _scope.lookupName(_get_scoped_name_prefix() + lookup_name);
    
    if(root_symb != nullptr)
    {
        _index->add_reference_to(*root_symb,node,_ref_filepath);
        return true;
    }
    return false;
}

std::string RefVisitor::_get_scoped_name_prefix()
{
    std::string ret;

    for(const std::string& elt : _scoped_name)
        ret += elt;

    if(!ret.empty())
        spdlog::debug("Used prefix {}",ret);
    return ret;
}


/**
 * @brief Register modules name as symbols
 * 
 * @param node 
 */
void RefVisitor::handle(const syntax::ModuleHeaderSyntax &node)
{
    // As we use the scope as a symbol, we can multiple "scope" and buffers for the same module.
    // Therefore, it is mandatory to lookup the module definition without using source range or token reference.
    // As file names are canonized, it can be used as reference for this lookup operation.
    if(!_in_instanciation)
    {
        // Try to lookup the syntax for the same location as the module name we are working on.
        std::optional<slang::syntax::ConstTokenOrSyntax> reference_node = _index->get_syntax_from_position(
            _sm->getFullPath(node.name.location().buffer()),
            _sm->getLineNumber(node.name.location()),
            _sm->getColumnNumber(node.name.location())
        );

        // If this file location, does not bring up any result, we can assume that the symbol is not registered.
        if(!reference_node)
        {
            // The _instance_type_x variable are used to help the reference lookup later on.
            _instance_type_symbol = &(_scope.asSymbol());
            _instance_type_name = node.name.rawText();
            _index->add_symbol(_scope.asSymbol(),node.name);
        }
        else
        {
            // The _instance_type_x variable are used to help the reference lookup later on.
            // Therefore it is required to set them up even if the symbol already exists.
            _instance_type_symbol = _index->get_symbol_from_exact_range(CONST_TOKNODE_SR(*reference_node));
            if(_instance_type_symbol.value() == nullptr)
                _instance_type_symbol.reset();
            _instance_type_name = node.name.rawText();
        }
    }
    visitDefault(node);
}

/**
 * @brief Handle the symbol of the type of an instance.
 * 
 * @param node 
 */
void RefVisitor::handle(const syntax::HierarchyInstantiationSyntax &node)
{
    if(_in_instanciation)
    {
        if(_instance_type_symbol && node.type.rawText() == _instance_type_name)
        {
            const slang::ast::Symbol* symb = _instance_type_symbol.value(); 
            _index->add_reference_to(*symb,node.type);
        }
    }
    visitDefault(node);
}


void RefVisitor::handle(const syntax::NamedPortConnectionSyntax &node)
{
    if(_in_instanciation)
    {
        _add_reference(syntax::ConstTokenOrSyntax(node.name),std::string(node.name.rawText()));
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
        _add_reference(syntax::ConstTokenOrSyntax(node.name),std::string(node.name.rawText()));
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


void RefVisitor::handle(const syntax::IdentifierSelectNameSyntax& node)
{
    _add_reference(syntax::ConstTokenOrSyntax(node.identifier), node.identifier.rawText());
    // You may have IdentifierName within the SelectName syntax (within the bracket) so 
    // you must visit the node anyway.
    visitDefault(node);
}

void RefVisitor::handle(const syntax::DeclaratorSyntax &node)
{
    _add_reference(node.name, node.name.rawText());
}

void RefVisitor::handle(const slang::syntax::ClassMethodDeclarationSyntax& node)
{
    // Bypass
}

void RefVisitor::handle(const slang::syntax::ScopedNameSyntax& node)
{
    // Aggregate the _scoped_name and then use the standard visitors to actually add references.
    if(_scoped_name_root == nullptr)
        _scoped_name_root = &node;

    visit(*(node.left));

    if(node.left->kind != syntax::SyntaxKind::ScopedName)
    {
        _scoped_name.push_back(node.left->toString() + std::string(node.separator.rawText()));
    }

    visit(*(node.right));
    
    if(_scoped_name_root == &node)
    {
        _scoped_name.clear();
        _scoped_name_root = nullptr;
    }
    
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

void IndexVisitor::handle(const slang::ast::ClassType& node)
{
    _index->add_symbol(node);
    visitDefault(node);

    const syntax::SyntaxNode* stx_node = node.getSyntax();

    if(stx_node != nullptr)
    {
        RefVisitor ref(_index.get(), node, _sm);
        stx_node->visit(ref);
    }

}

// void IndexVisitor::handle(const slang::ast::ClassPropertySymbol& node)
// {
//     _index->add_symbol(node);
// }

void IndexVisitor::handle(const slang::ast::SubroutineSymbol& node)
{
    _index->add_symbol(node);
    visitDefault(node);
}

void IndexVisitor::handle(const slang::ast::FormalArgumentSymbol& node)
{
    int i;
    if(node.name == "newval")
        i = 0;
    _index->add_symbol(node);
}

void IndexVisitor::handle(const slang::ast::Scope& node)
{
    const slang::syntax::SyntaxNode* covered_syntax = node.asSymbol().getSyntax();
    visitDefault(node);
    
    if(covered_syntax != nullptr)
    {

        RefVisitor ref(_index.get(), node, _sm, &_explored);
        if(ref.is_valid())
            covered_syntax->visit(ref);

        _explored.emplace(covered_syntax);
    }
}

}
