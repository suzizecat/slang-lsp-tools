#pragma once 

#include <numeric>
#include <string>

#include "slang/util/BumpAllocator.h"
#include "slang/parsing/Token.h"

class IndentManager
{
 protected:
    unsigned int _level;
    unsigned int _space_per_level;

    bool _use_tabs;

    slang::BumpAllocator& _mem;
 public:
    IndentManager(slang::BumpAllocator& alloc, const unsigned int space_per_level, const bool use_tabs );

    slang::parsing::Token replace_spacing(const slang::parsing::Token& tok, unsigned int spaces);
    slang::parsing::Token indent(const slang::parsing::Token& tok);

    inline void add_level(unsigned int to_add = 1) {_level += to_add;};
    inline void sub_level(unsigned int to_sub = 1) {_level -= std::min(to_sub,_level);};
};