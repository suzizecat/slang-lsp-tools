#pragma once 

#include <numeric>
#include <string>

#include "slang/util/BumpAllocator.h"
#include "slang/parsing/Token.h"
#include "slang/syntax/AllSyntax.h"

class SpacingManager
{
 protected:
    unsigned int _level;
    unsigned int _space_per_level;

    bool _use_tabs;

    slang::BumpAllocator& _mem;
 public:
   SpacingManager(slang::BumpAllocator& alloc, const unsigned int space_per_level, const bool use_tabs );

   slang::parsing::Token replace_spacing(const slang::parsing::Token& tok, int spaces);
   slang::parsing::Token remove_spacing(const slang::parsing::Token& tok);
   slang::parsing::Token indent(const slang::parsing::Token& tok, int additional_spacing = 0);
   slang::parsing::Token token_align_right(const slang::parsing::Token& tok, unsigned int align_size, bool allow_no_space = true);
   
   slang::parsing::Token replace_comment_spacing(const slang::parsing::Token& tok, int spaces);
   slang::parsing::Token remove_extra_newlines(const slang::parsing::Token& tok, bool clear_all);

   unsigned int align_dimension(slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment);
   unsigned int align_dimension(slang::syntax::SyntaxList<slang::syntax::ElementSelectSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment);

   inline void add_level(unsigned int to_add = 1) {_level += to_add;};
   inline void sub_level(unsigned int to_sub = 1) {_level -= std::min(to_sub,_level);};
};


struct IndentLock
{
   IndentLock(SpacingManager& manager, unsigned int level_to_add = 1U);
   ~IndentLock();

   protected:
      SpacingManager& _managed;
      unsigned int _added_level;
};


std::string raw_text_from_syntax(const slang::syntax::SyntaxNode& node);
std::string raw_text_from_syntax(const slang::syntax::SyntaxNode* node);