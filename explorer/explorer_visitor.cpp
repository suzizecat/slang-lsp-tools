#include "explorer_visitor.h"

#include <iostream>
using namespace slang::syntax;



// template<>
// void SVvisitor::handle<ImplicitAnsiPortSyntax>(const ImplicitAnsiPortSyntax& node)
// {
//     std::cout << "Port : " << node.toString() << std::endl;
// }

void SVvisitor::handle(const ImplicitAnsiPortSyntax& node)
{
    std::cout << "Port : " << node.declarator->name.valueText() << std::endl;
}

