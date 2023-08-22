#include "sv_document.hpp"

SVDocument::SVDocument(std::string path) : sm()
{
    st = slang::syntax::SyntaxTree::fromFile(path,sm).value();
}

const std::string SVDocument::get_module_name()
{
    return bb.value_or(_compute_module_bb()).module_name;
}


const ModuleBlackBox& SVDocument::_compute_module_bb()
{
    VisitorModuleBlackBox visitor;
    st->root().visit(visitor);

    bb = visitor.bb;
    return bb.value();
}