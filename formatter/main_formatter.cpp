#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"
#include "ast_print.hpp"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include "slang/text/SourceManager.h"
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include "slang/util/BumpAllocator.h"
#include <iostream>
#include "fmt/format.h"

#include "slang/syntax/SyntaxPrinter.h"
#include "spacing_manager.hpp"
#include "format_DataDeclaration.hpp"


int main(int argc, char** argv) {
    argparse::ArgumentParser prog("systemverilog code formatter", "0.0.2");
    prog.add_argument("file")
        .help("File path");
    prog.add_argument("--use-tabs","-t")
        .help("Use tabs instead of spaces for indentation")
        .flag();
    prog.add_argument("--spacing","-s")
        .help("Number of spaces for one level of indent")
        .scan<'u',unsigned int>()
        .default_value(4U);
    prog.add_argument("--debug","-d")
        .help("Print the AST before and after the formatting")
        .flag();

    try {
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }
    bool debug = prog.get<bool>("--debug");
    slang::SourceManager sm;
    auto st = slang::syntax::SyntaxTree::fromFile(prog.get<std::string>("file"),sm).value();

    if(debug)
    {
        std::cout << slang::syntax::SyntaxPrinter::printFile(*st) << std::endl << std::endl;
         print_tokens(&(st->root()));
    }
    
    slang::BumpAllocator mem;
    SpacingManager idt(mem,prog.get<unsigned int>("--spacing"),prog.get<bool>("--use-tabs"));
    
    idt.add_level();
    
    DataDeclarationSyntaxVisitor fmter(&idt);
    std::shared_ptr<slang::syntax::SyntaxTree> formatted = fmter.transform(st);
    if(debug)
    {
        std::cout << "POST - FORMAT AST ################" << std::endl << std::endl;
        print_tokens(&(formatted->root()));
    }

    std::cout << slang::syntax::SyntaxPrinter::printFile(*formatted) << std::endl << std::endl;
    //std::cout << fmter.formatted << std::endl;
    return 0;
}
