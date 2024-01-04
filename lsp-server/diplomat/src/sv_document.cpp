#include "sv_document.hpp"
#include <istream>


using namespace slang;

SVDocument::SVDocument(std::string path, slang::SourceManager* sm) : sm(sm)
{
    st = slang::syntax::SyntaxTree::fromFile(path,*sm).value();
    for (BufferID id : sm->getAllBuffers())
    {
        if (sm->getFullPath(id) == std::filesystem::path(path))
        {
            _buff_id = id;
            break;
        }
    }
}

const std::string SVDocument::get_module_name()
{
    return bb.value_or(_compute_module_bb()).module_name;
}

int SVDocument::buffer_position_from_location(int line, int column)
{
    if (!_line_size_cache)
        _update_line_cache();

    return _line_size_cache.value().at(line - 1) + column;
}

slsp::types::Position SVDocument::position_from_slang(const slang::SourceLocation& pos)
{
    slsp::types::Position ret;

    if (!_line_size_cache)
        _update_line_cache();

    int sel_line = 0;
    while (sel_line < _line_size_cache.value().size() && _line_size_cache.value().at(sel_line) < pos.offset())
    {
        sel_line++;
    }

    ret.line = sel_line;
    ret.character = pos.offset() - (sel_line > 0 ? _line_size_cache.value().at(sel_line - 1) : 0);

    return ret;
}

slsp::types::Range SVDocument::range_from_slang(const slang::SourceLocation& start, const slang::SourceLocation& end)
{
    return slsp::types::Range{position_from_slang(start),position_from_slang(end)};
}

slsp::types::Range SVDocument::range_from_slang(const slang::SourceRange& range)
{
    return slsp::types::Range{position_from_slang(range.start()),position_from_slang(range.end())};
}

const ModuleBlackBox& SVDocument::_compute_module_bb()
{
    VisitorModuleBlackBox visitor;
    st->root().visit(visitor);

    bb = visitor.bb;
    return bb.value();
}

/**
 * @brief Refresh the line position caching
 * 
 * To improve performances, the end of line position is saved in a map.
 * This allows a very simple binding to get the offset of the first char of line "pos"
 * 
 */
void SVDocument::_update_line_cache()
{
    // There is only one buffer.
    _line_size_cache.reset();
    _line_size_cache = std::vector<unsigned int>();
    unsigned int pos = 0;
    std::string fcontent = std::string(sm->getSourceText(_buff_id));

    std::istringstream iss(fcontent);
    for (std::string line; std::getline(iss,line); )
    {
        pos += line.length() +1 ;
        _line_size_cache.value().push_back(pos);
    }

}

// std::optional<const syntax::SyntaxNode&> SVDocument::get_syntax_node_from_location(const SourceLocation& pos)
// {
    
//     const syntax::SyntaxNode& root = st.get()->root();
    
//     std::stack<const syntax::SyntaxNode*> position;
//     position.push(&root);
    
//     while (position.top()->getChildCount() > 0)
//     {
//         const syntax::SyntaxNode* node = position.top();
//         const syntax::SyntaxNode* selected = nullptr;

//         for (int i = 0; i < node->getChildCount(); i++)
//         {
//             const syntax::SyntaxNode* child_node = node->childNode(i);
//             // If we got a token instead of a proper node, just skip
//             if (child_node == nullptr)
//                 continue;

//             const slang::SourceLocation pos_start = child_node->sourceRange().start();
//             const slang::SourceLocation pos_end = child_node->sourceRange().end();

//             if (pos_start.buffer() != pos.buffer())
//                 continue;
//             if (pos_start > pos || pos > pos_end)
//                 continue;

//             selected = child_node;
//         }

//         if (selected != nullptr)
//             position.push(selected);
//         else
//         {
//             break;
//         }
//     }
    
//     if (position.size() == 1)
//     {
//         std::optional<const syntax::SyntaxNode&> empty_return;
//         return empty_return;
//     }
//     else
//     {
//         return *(position.top());
//     }
// }
