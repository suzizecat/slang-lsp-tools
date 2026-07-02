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
class IndentationPhaseVisitor : public slang::syntax::SyntaxRewriter<IndentationPhaseVisitor>
{
    protected:
        slang::BumpAllocator _mem;
		SpacingManager* _idt;

        bool _has_done_work; // Indent update has been done.
		bool _default_indent_handler(const slang::syntax::SyntaxNode &node);
        
    public:
        explicit IndentationPhaseVisitor(SpacingManager* idt);
        void handle(const slang::syntax::ModuleDeclarationSyntax &node);
        void handle(const slang::syntax::ProceduralBlockSyntax &node);
        void handle(const slang::syntax::ConditionalStatementSyntax &node);
		
        void handle(const slang::syntax::DataDeclarationSyntax &node);
        void handle(const slang::syntax::MemberSyntax &node);
        void handle(const slang::syntax::ExpressionSyntax &node);

        inline bool has_done_work(bool reset = true) { 
            bool to_ret = _has_done_work;
            if(reset){
                _has_done_work = false;
            }
            return to_ret;
        }
};
