#ifndef _H_INCLUDE_EXPLORER_VISITOR
#define _H_INCLUDE_EXPLORER_VISITOR
#include "slang/syntax/AllSyntax.h"
#include <slang/syntax/SyntaxVisitor.h>


class SVvisitor : public slang::syntax::SyntaxVisitor<SVvisitor>
{
    
    public : 
    //void handle(const slang::syntax::ModuleDeclarationSyntax& node);
    void handle(const slang::syntax::ImplicitAnsiPortSyntax& node);
    // template<typename T>
    // void handle(const T& node); // Base function, specialized in C++

};

#endif //_H_INCLUDE_EXPLORER_VISITOR