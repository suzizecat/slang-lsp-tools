#pragma once 

#include <iostream>

#include "fmt/format.h"

#include "slang/syntax/SyntaxNode.h"
#include "slang/syntax/SyntaxTree.h"


void print_tree_elt(const std::string_view elt_title,const std::string_view elt_value, const int level, const std::string lead = " ");
void print_slang_cst(const slang::syntax::SyntaxNode* node, int level = 0);