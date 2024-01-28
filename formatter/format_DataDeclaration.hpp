#pragma once

#include <string>
#include <memory>
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxVisitor.h"
#include <slang/util/BumpAllocator.h>

#include <map>
#include <vector>
#include <set>

#include "indent_manager.hpp"

/**
 * @brief An integer type description will be shown as <type> {[<high>:<low>]} with high and low different sizes.
 * For a given line, we store the length of all <high> and <low> sequencially and 
 * 
 */
class DataDeclarationSyntaxVisitor : public slang::syntax::SyntaxRewriter<DataDeclarationSyntaxVisitor>
{
    protected:
        // logic [3:0][1:0] my_var [3:0][2:0];
        // type   type sz    var    array sz
        std::vector<size_t> _type_sizes; 
        std::vector<size_t> _array_sizes;
        size_t _type_name_size;
        size_t _var_name_size;

        slang::BumpAllocator _mem;

        // Used to detect a change of kind in a middle of a bloc.
        slang::syntax::SyntaxKind _bloc_type_kind; 

        std::vector<const slang::syntax::DataDeclarationSyntax*> _to_format;

        IndentManager* _idt;

        void _store_format(const slang::syntax::DataDeclarationSyntax& node);
        std::string _format(const slang::syntax::DataDeclarationSyntax* decl);
        void _read_type_array(const slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax> dimensions);
        void _debug_print();
    public:
        explicit DataDeclarationSyntaxVisitor(IndentManager* idt);
        void process_pending_formats();
        void clear();
        void handle(const slang::syntax::DataDeclarationSyntax& node);
        std::string formatted;
};

unsigned int type_length(const slang::syntax::IntegerTypeSyntax* node);
void  dimension_syntax_to_vector(const slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax> dimensions, std::vector<size_t>& target_vector);
std::string raw_text_from_syntax(const slang::syntax::SyntaxNode& node);

slang::parsing::Token token_align_right( slang::BumpAllocator& alloc ,const slang::parsing::Token& tok, unsigned int align_size, bool allow_no_space = true);
slang::parsing::Token replace_spacing( slang::BumpAllocator& alloc ,const slang::parsing::Token& tok, unsigned int space_number);
slang::parsing::Token indent( slang::BumpAllocator& alloc ,const slang::parsing::Token& tok, const char indent_char, unsigned int char_num, unsigned int indent_level);

unsigned int align_dimension(slang::BumpAllocator& alloc,slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax>& dimensions,const std::vector<size_t>& sizes_refs, const int first_alignment);