#include "explorer_visitor.h"

#include "slang/ast/Compilation.h"
#include "slang/text/SourceManager.h"
#include "fmt/format.h"
#include <iostream>
namespace ast = slang::ast;
namespace syntax = slang::syntax;
using json = nlohmann::json;


RefVisitor::RefVisitor(ref_table_t* ref_storage, const ast::Scope& scope, const slang::SourceManager* sm) : 
    syntax::SyntaxVisitor<RefVisitor>(), 
    _sm(sm),
    _scope(scope),
    _ref_storage(ref_storage),
    in_instanciation(false)
{}



bool RefVisitor::_add_reference(const syntax::SyntaxNode& node, const std::string_view lookup_name)
{
    return _add_reference(node,std::string(lookup_name));
}

bool RefVisitor::_add_reference(const syntax::SyntaxNode& node, const std::string& lookup_name)
{    
    return _add_reference(node.sourceRange(),lookup_name);
}

bool RefVisitor::_add_reference(const slang::SourceRange& loc, const std::string& lookup_name)
{    
    const ast::Symbol* root_symb = _scope.lookupName(lookup_name);
    if(root_symb != nullptr && _ref_storage->contains(root_symb))
    {
        _ref_storage->at(root_symb).push_back(fmt::format("{}:{}:{}", 
                                                        _sm->getFileName(loc.start()),
                                                        _sm->getLineNumber(loc.start()),
                                                        _sm->getColumnNumber(loc.start()) ));

        return true;
    }
    else
    {
        return false;
    }
}

void RefVisitor::handle(const syntax::NamedPortConnectionSyntax &node)
{
    if(in_instanciation)
    {
        _add_reference(node.name.range(),std::string(node.name.rawText()));
    }
    else
    {
        visitDefault(node);
    }
}

void RefVisitor::handle(const syntax::NamedParamAssignmentSyntax &node)
{
    if(in_instanciation)
    {
        _add_reference(node.name.range(),std::string(node.name.rawText()));
    }
    else
    {
        visitDefault(node);
    }
}


void RefVisitor::handle(const syntax::IdentifierNameSyntax &node)
{
    if(! _add_reference(node, node.identifier.rawText()))
        visitDefault(node);
}


void ExplorerVisitor::handle(const slang::ast::InstanceSymbol &node)
{
    _pointer.push_back(std::string(node.name));
    visitDefault(node);
    RefVisitor ref(&_ref_storage,node.body,node.body.getCompilation().getSourceManager());
    

    const auto* stx_node = node.body.getSyntax();

    if(stx_node != nullptr)
    {
        stx_node->visit(ref);
    }

    stx_node = node.getSyntax();
    ref.in_instanciation = true;

    if(stx_node != nullptr)
    {
        stx_node->parent->visit(ref);
    }
    
    for (auto& [symb, refs] : _ref_storage) 
    {
        for(auto& refpath : refs)
        {
            _refs[_pointer/std::string(symb->name)/"refs"].push_back(refpath);
        }
    }
    _ref_storage.clear();
    _pointer.pop_back();
}


void ExplorerVisitor::handle(const ast::ValueSymbol &node)
{
    std::string name(node.name);
    _pointer.push_back(name);
    if(! _ref_storage.contains(&node))
    {
        _ref_storage[&node] = {};
    }
    const auto* stx_node = node.getSyntax();
    const slang::SourceManager* sm = node.getParentScope()->getCompilation().getSourceManager();

    _refs[_pointer/"location"] = fmt::format("{}:{}:{} ", sm->getFileName(node.location),sm->getLineNumber(node.location),sm->getColumnNumber(node.location));


    // if (stx_node != nullptr)
    // {
    //     slang::SourceRange pos = stx_node->sourceRange();
    //     _refs[_pointer/"ref"/"location"] = fmt::format("{}:{}:{}", sm->getFileName(pos.start()),sm->getLineNumber(pos.start()),sm->getColumnNumber(pos.start()));
    //     // _refs[_pointer/"ref"/"f"] = sm->getFileName(pos.start());
    //     // _refs[_pointer/"ref"/"ls"] = sm->getLineNumber(pos.start());
    //     // _refs[_pointer/"ref"/"cs"] = sm->getColumnNumber(pos.start());
    //     // _refs[_pointer/"ref"/"le"] = sm->getLineNumber(pos.end());
    //     // _refs[_pointer/"ref"/"ce"] = sm->getColumnNumber(pos.end());

    //     // std::cout << sm->getFileName(pos.start()) << ":" << sm->getLineNumber(pos.start()) << ":" << sm->getColumnNumber(pos.start());
    //     // std::cout                                 << ":" << sm->getLineNumber(pos.end()) << ":" << sm->getColumnNumber(pos.end());
    // }
    // // std::cout << std::endl;

    _pointer.pop_back();
}


