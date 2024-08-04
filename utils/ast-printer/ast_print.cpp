#include "ast_print.hpp"

#include "slang/text/SourceManager.h"

using namespace slang;

void print_tree_elt(const std::string_view elt_title,const std::string_view elt_value, const int level, const std::string lead)
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

void print_tokens(const syntax::SyntaxNode* node, int level)
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

