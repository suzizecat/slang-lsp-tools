#pragma once

#include <optional>
#include <string>
#include <memory>

#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

#include "visitor_module_bb.hpp"

struct SVDocument
{
    public:

    slang::SourceManager sm;
    std::shared_ptr<slang::syntax::SyntaxTree > st;
    
    SVDocument(std::string path);
    const std::string get_module_name();

    std::optional<ModuleBlackBox> bb;
    
    protected:
    
    const ModuleBlackBox& _compute_module_bb();
};
