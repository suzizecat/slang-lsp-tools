#pragma once

#include <optional>
#include <string>
#include <memory>

#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"
#include "slang/text/SourceLocation.h"
#include "slang/syntax/SyntaxNode.h"

#include "types/structs/Position.hpp"
#include "types/structs/Range.hpp"

#include "visitor_module_bb.hpp"

struct SVDocument
{
    public:

    slang::SourceManager* sm;
    std::shared_ptr<slang::syntax::SyntaxTree > st;
    
    SVDocument(std::string path, slang::SourceManager* sm);
    const std::string get_module_name();
    int buffer_position_from_location(int line, int column);
    
    slsp::types::Position position_from_slang(const slang::SourceLocation& pos);
    slsp::types::Range range_from_slang(const slang::SourceLocation& start,const slang::SourceLocation& end);
    slsp::types::Range range_from_slang(const slang::SourceRange& range);

    // std::optional<const slang::syntax::SyntaxNode&>  get_syntax_node_from_location(const slang::SourceLocation& pos);

    std::optional<ModuleBlackBox> bb;
    std::optional<std::string> doc_uri;

    protected:
    slang::BufferID _buff_id;
    std::optional<std::vector<unsigned int> > _line_size_cache;
    const ModuleBlackBox& _compute_module_bb();
    void _update_line_cache();
};
