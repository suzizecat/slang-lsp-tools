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
    int buffer_position_from_location(int line, int column);
    
    std::optional<ModuleBlackBox> bb;

    protected:
    std::optional<std::vector<unsigned int> > _line_size_cache;
    const ModuleBlackBox& _compute_module_bb();
    void _update_line_cache();
};
