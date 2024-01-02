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
        std::size_t _decl_name_width;
        std::size_t _decl_type_width;
        std::size_t _decl_sttm_width;
        std::vector<std::string> _param_declarations;
        std::string _indent();

        void _set_parameter_sttm_sizes(const slang::syntax::ParameterPortListSyntax& node);
        void _set_parameter_sttm_sizes(const slang::syntax::AnsiPortListSyntax& node);
    public:
        std::string _content;
        
        explicit SVFormatter();
        void handle(const slang::syntax::ModuleDeclarationSyntax& node);
        void handle(const slang::syntax::ModuleHeaderSyntax& node);
        void handle(const slang::syntax::ParameterPortListSyntax& node);
        void handle(const slang::syntax::ParameterDeclarationSyntax& node);
        void handle(const slang::syntax::AnsiPortListSyntax& node);
};