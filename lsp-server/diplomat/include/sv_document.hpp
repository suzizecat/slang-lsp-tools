#pragma once

#include <string>
#include <memory>

#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"


struct SVDocument
{
    slang::SourceManager sm;
    std::shared_ptr<slang::syntax::SyntaxTree > st;

    SVDocument(std::string path);
};
