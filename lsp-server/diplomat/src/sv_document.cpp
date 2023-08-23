#include "sv_document.hpp"
#include <istream>
SVDocument::SVDocument(std::string path) : sm()
{
    st = slang::syntax::SyntaxTree::fromFile(path,sm).value();
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

const ModuleBlackBox& SVDocument::_compute_module_bb()
{
    VisitorModuleBlackBox visitor;
    st->root().visit(visitor);

    bb = visitor.bb;
    return bb.value();
}

void SVDocument::_update_line_cache()
{
    // There is only one buffer.
    _line_size_cache.reset();
    _line_size_cache = std::vector<unsigned int>();
    unsigned int pos = 0;
    std::string fcontent = std::string(sm.getSourceText(sm.getAllBuffers().at(0)).data());

    std::istringstream iss(fcontent);
    for (std::string line; std::getline(iss,line); )
    {
        pos += line.length() +1 ;
        _line_size_cache.value().push_back(pos);
    }

}
