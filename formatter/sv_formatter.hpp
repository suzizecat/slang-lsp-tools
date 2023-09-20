#pragma once

#include <string>
#include <memory>
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxVisitor.h"


class SVFormatter : public slang::syntax::SyntaxVisitor<SVFormatter>
{
    protected:
        int _indentation_level;
        std::string _indent();
    public:
        std::string _content;
        
        explicit SVFormatter();
        void handle(const slang::syntax::ModuleDeclarationSyntax& node);
        void handle(const slang::syntax::ModuleHeaderSyntax& node);
        void handle(const slang::syntax::ParameterPortListSyntax& node);
        void handle(const slang::syntax::ParameterDeclarationSyntax& node);
};