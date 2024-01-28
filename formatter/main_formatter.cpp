#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"
#include "sv_formatter.hpp"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxNode.h"
#include "slang/text/SourceManager.h"
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include "slang/util/BumpAllocator.h"
#include <iostream>
#include "fmt/format.h"

#include "indent_manager.hpp"
#include "format_DataDeclaration.hpp"
using namespace slang;

void print_tree_elt(const std::string_view elt_title,const std::string_view elt_value, const int level, const std::string lead = " ")
{
    std::string title = "";
    for(int i = 0; i < level; i++)
        title += "| ";
    title += lead;
    title += elt_title;

    if(elt_value == "")
        std::cout << title << std::endl;
    else
        std::cout << fmt::format("{:.<70s}: {}",title,elt_value) << std::endl;
}

void print_tokens(const syntax::SyntaxNode* node, int level = 0)
{
    const syntax::SyntaxNode* next;
    print_tree_elt(syntax::toString(node->kind),"",level,">");
    for(int i = 0; i < node->getChildCount(); i++)
    {
        if((next = node->childNode(i)) != nullptr)
        {
            print_tokens(next,level +1);
        } else
        {
            parsing::Token t = node->childToken(i);
            
            if(! t.isMissing() && t.kind != parsing::TokenKind::Unknown)
            {
                    print_tree_elt(slang::parsing::toString(t.kind),t.valueText(),level + 1, "-");
                    if(t.trivia().size() > 0)
                    {
                        for(parsing::Trivia trivia : t.trivia())
                        {
                            if(! trivia.valid())
                                continue ;
                            else
                                print_tree_elt(slang::parsing::toString(trivia.kind),"",level+2,"~");
                            if((next = trivia.syntax())!= nullptr)
                            {
                                print_tokens(next,level+2);
                            }
                            for (parsing::Token triv_tok : trivia.getSkippedTokens())
                            {
                                print_tree_elt( slang::parsing::toString(triv_tok.kind),triv_tok.valueText(), level +2, "-");
                            }
                            //std::cout << trivia.getRawText();
                        }
                        //std::cout << std::endl;
                    } 
            }
        }
    }
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", "0.0.1");
    prog.add_argument("file")
        .help("File path");

    try {
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }
    slang::SourceManager sm;
    auto st = slang::syntax::SyntaxTree::fromFile(prog.get<std::string>("file"),sm).value();
    //SVFormatter fmter;
    print_tokens(&(st->root()));
    
    BumpAllocator mem;
    IndentManager idt(mem,4,false);
    
    idt.add_level();

    DataDeclarationSyntaxVisitor fmter(&idt);
    st->root().visit(fmter);
    fmter.process_pending_formats();
    std::cout << fmter.formatted << std::endl;
    return 0;
}
