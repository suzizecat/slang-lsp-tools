
#include "ast_print.hpp"

#include "slang/text/SourceManager.h"

using namespace slang;

int main(int argc, char** argv) {

    argparse::ArgumentParser prog("systemverilog code formatter", "0.0.2");
    prog.add_argument("file")
        .help("File path");

    try {
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << prog << std::endl;
        std::exit(1);
    }

    slang::SourceManager sm;
    auto st = slang::syntax::SyntaxTree::fromFile(prog.get<std::string>("file"),sm).value();

    print_tokens(&(st->root()));

    return 0;
}