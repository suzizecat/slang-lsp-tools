#ifndef _H_DOCUMENTER_VISITOR_DEF
#define _H_DOCUMENTER_VISITOR_DEF

#include "slang/syntax/SyntaxVisitor.h"
#include "slang/syntax/AllSyntax.h"
#include "nlohmann/json.hpp"


using json = nlohmann::json;
class DocumenterVisitor : public slang::syntax::SyntaxVisitor<DocumenterVisitor>
{

protected:
        bool _only_modules;
        const slang::SourceManager* _sm;

public:
        explicit DocumenterVisitor(bool only_modules = false, const slang::SourceManager* sm = nullptr);
        void handle(slang::syntax::ModuleHeaderSyntax &node);
        void handle(slang::syntax::NonAnsiPortSyntax &port);
        void handle(slang::syntax::NonAnsiPortListSyntax &port);
        void handle(slang::syntax::ImplicitAnsiPortSyntax &port);
        void handle(slang::syntax::ParameterDeclarationSyntax &node);

        void set_source_manager(const slang::SourceManager *new_sm);
        json data;
        json module;
};

#endif  //_H_DOCUMENTER_VISITOR_DEF