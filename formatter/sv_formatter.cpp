#include "sv_formatter.hpp"
#include "fmt/format.h"
#include <iostream>

using namespace slang;

SVFormatter::SVFormatter():
    _indentation_level(0)
{}

std::string SVFormatter::_indent()
{
    std::string ret;
    for(int i = 0; i < _indentation_level; i++)
        ret += "\t";
    
    return ret;
}

void SVFormatter::handle(const slang::syntax::ModuleDeclarationSyntax& node)
{
    // std::cout << _indent() << "Enter " << slang::syntax::toString(node.kind) <<std::endl;
    visitDefault(node);
    _indentation_level --;
    _content += fmt::format("{}endmodule\n",_indent());
}

void SVFormatter::handle(const slang::syntax::ModuleHeaderSyntax& node)
{
    // std::cout << _indent() << "Enter " << slang::syntax::toString(node.kind) <<std::endl;
    std::string_view module_name = node.name.valueText();
    _content += fmt::format("{}module {}",_indent(), module_name);

    visitDefault(node);
    _indentation_level ++;
}

void SVFormatter::handle(const slang::syntax::ParameterPortListSyntax& node)
{
    // std::cout << _indent() << "Enter " << slang::syntax::toString(node.kind) <<std::endl;
    if(node.declarations.size() > 0)
    {
        _content += " #(\n";
        _indentation_level ++;
        visitDefault(node);
        _indentation_level --;
        _content += fmt::format("{}) ",_indent());
    }    
}

void SVFormatter::handle(const slang::syntax::ParameterDeclarationSyntax& node)
{
    // std::cout << _indent() << "Enter " << slang::syntax::toString(node.kind) <<std::endl;
    for(const syntax::DeclaratorSyntax* decl : node.declarators)
    {
        _content += fmt::format("{}parameter {}",_indent(),decl->name.rawText());
        if(decl->initializer != nullptr)
        {
            std::cout << syntax::toString(decl->initializer->expr->kind) << std::endl;
            _content += fmt::format(" = {}",decl->initializer->expr->toString());
        }

        _content += ",\n";
        
        /*
        if(decl != node.declarators.end().dereference())
            _content += ",\n";
        else 
            _content += "\n";
        */
    }
}