 #include "sv_document.hpp"

SVDocument::SVDocument(std::string path) : sm()
{
    st = slang::syntax::SyntaxTree::fromFile(path,sm).value();
}
