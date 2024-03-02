#pragma once

#include <string>
#include <memory>
#include <ranges>
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include "slang/syntax/AllSyntax.h"
#include "slang/syntax/SyntaxVisitor.h"
#include "slang/util/BumpAllocator.h"

#include "formatter_utils.hpp"

#include <map>
#include <vector>
#include <set>

#include "spacing_manager.hpp"

/**
 * @brief An integer type description will be shown as <type> {[<high>:<low>]} with high and low different sizes.
 * For a given line, we store the length of all <high> and <low> sequencially and 
 * 
 */
class DataDeclarationSyntaxVisitor : public slang::syntax::SyntaxRewriter<DataDeclarationSyntaxVisitor>
{
    protected:
        // static const logic [3:0][1:0] my_var [3:0][2:0];
        // modifier sz  type   type sz    var    array sz
        std::vector<size_t> _modifier_sizes; 
        std::vector<std::pair<size_t,size_t>> _type_sizes; 
        std::vector<std::pair<size_t,size_t>> _array_sizes;
        size_t _type_name_size;
        size_t _var_name_size;
        
        // .name(value) for parameters and ports
        size_t _param_name_size;
        size_t _param_value_size;
        size_t _port_name_size;
        size_t _port_value_size;

        slang::BumpAllocator _mem;

        std::vector<unsigned int> _remaining_alignment;

        // Used to detect a change of kind in a middle of a bloc.
        slang::syntax::SyntaxKind _bloc_type_kind; 

        std::vector<const slang::syntax::SyntaxNode*> _to_format;

        SpacingManager* _idt;

        slang::syntax::SyntaxNode* _format_any(const slang::syntax::SyntaxNode* node);

        slang::syntax::DataDeclarationSyntax* _format(const slang::syntax::DataDeclarationSyntax& decl);
        slang::syntax::ImplicitAnsiPortSyntax* _format(const slang::syntax::ImplicitAnsiPortSyntax& decl);
        slang::syntax::HierarchyInstantiationSyntax* _format(const slang::syntax::HierarchyInstantiationSyntax& decl);
        size_t _format_data_type_syntax(slang::syntax::DataTypeSyntax* stx, bool first_element, size_t first_alignement_size);
        void _split_bloc(const slang::syntax::SyntaxNode& node);
        void _read_type_len(const slang::syntax::DataTypeSyntax* type);
        void _store_format(const slang::syntax::DataDeclarationSyntax& node);
        void _store_format(const slang::syntax::ImplicitAnsiPortSyntax& node);
        void _read_type(const slang::syntax::DataDeclarationSyntax& node);
        void _read_type(const slang::syntax::ImplicitAnsiPortSyntax &node);
        void _switch_bloc_type(const slang::syntax::SyntaxNode &node, bool force = false);
        void _switch_bloc_type(const slang::syntax::SyntaxNode *node, bool force = false);

        public:
        explicit DataDeclarationSyntaxVisitor(SpacingManager *idt);
        void process_pending_formats();
        void clear();
        void handle(const slang::syntax::CompilationUnitSyntax &node);
        void handle(const slang::syntax::HierarchyInstantiationSyntax &node);
        void handle(const slang::syntax::NamedParamAssignmentSyntax &node);
        void handle(const slang::syntax::NamedPortConnectionSyntax &node);
        void handle(const slang::syntax::AnsiPortListSyntax &node);
        void handle(const slang::syntax::DataDeclarationSyntax &node);
        void handle(const slang::syntax::ImplicitAnsiPortSyntax &node);
        void handle(const slang::syntax::ContinuousAssignSyntax &node);
};

unsigned int type_length(const slang::syntax::IntegerTypeSyntax* node);
void  dimension_syntax_to_vector(const slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax> dimensions, std::vector<std::pair<size_t,size_t>>& target_vector);
void  dimension_syntax_to_vector(const slang::syntax::SyntaxList<slang::syntax::ElementSelectSyntax> dimensions, std::vector<std::pair<size_t,size_t>>& target_vector);
void  tokens_to_vector(const std::span<const slang::parsing::Token> toks, std::vector<size_t>& target_vector);


