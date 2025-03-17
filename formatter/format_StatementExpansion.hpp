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
class StatementExpansionPhaseVisitor : public slang::syntax::SyntaxRewriter<StatementExpansionPhaseVisitor>
{
    protected:
        slang::BumpAllocator _mem;
		SpacingManager* _idt;
        
        public:
        explicit StatementExpansionPhaseVisitor(SpacingManager* idt);
        void handle(const slang::syntax::DataDeclarationSyntax &node);
        //void handle(const slang::syntax::ContinuousAssignSyntax &node);
};
