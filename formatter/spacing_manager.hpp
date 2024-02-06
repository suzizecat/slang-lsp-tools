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
   slang::parsing::Token indent(const slang::parsing::Token& tok, int additional_spacing = 0);
   slang::parsing::Token token_align_right(const slang::parsing::Token& tok, unsigned int align_size, bool allow_no_space = true);
   
   unsigned int align_dimension(slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment);
   unsigned int align_dimension(slang::syntax::SyntaxList<slang::syntax::ElementSelectSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment);

   inline void add_level(unsigned int to_add = 1) {_level += to_add;};
   inline void sub_level(unsigned int to_sub = 1) {_level -= std::min(to_sub,_level);};
};