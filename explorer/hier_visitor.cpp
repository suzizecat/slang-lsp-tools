#include "hier_visitor.h"
#include <iostream>
using json = nlohmann::json;
namespace ast = slang::ast;



void HierVisitor::handle(const slang::ast::InstanceSymbol &node)
{
    _pointer.push_back(std::string(node.name));
    _hierarchy[_pointer/"in"] = json::array();
    _hierarchy[_pointer/"out"] = json::array();
    _hierarchy[_pointer/"def"] = true;
    visitDefault(node);
    _pointer.pop_back();
}

void HierVisitor::handle(const slang::ast::PortSymbol& node)
{
    if(node.direction == ast::ArgumentDirection::In)
    {
        _hierarchy[_pointer/"in"].push_back(node.name);
    }
    else if(node.direction == ast::ArgumentDirection::Out)
    {
        _hierarchy[_pointer/"out"].push_back(node.name);
    }
    //visitDefault(node);
}

void HierVisitor::handle(const slang::ast::UninstantiatedDefSymbol& node)
{
    _pointer.push_back(std::string(node.name));
    _hierarchy[_pointer/"def"] = false;
    _pointer.pop_back();
}

/*
void HierVisitor::handle(const slang::ast::ValueSymbol& node)
{
    std::cout << "Value symbol " << node.name << " of kind "<< ast::toString(node.kind) << " driven by " << std::endl;
    for(const auto* drv : node.drivers())
    {
        std::cout << "    " << ast::toString(drv->kind) << " "  << drv->containingSymbol->name << " (" << ast::toString(drv->containingSymbol->kind) << ") " << std::endl;
    }
    visitDefault(node);
}
*/