#pragma once

#include <functional>
#include "slang/syntax/SyntaxNode.h"
#include "slang/text/SourceLocation.h"
#include "slang/parsing/Token.h"

template<>
struct std::hash<slang::SourceRange>
{
    std::size_t operator()(const slang::SourceRange& l) const noexcept
    {
        std::size_t h1 = std::hash<slang::SourceLocation>{}(l.start());
        std::size_t h2 = std::hash<slang::SourceLocation>{}(l.end());
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};


template<>
struct std::hash<slang::parsing::Token>
{
    std::size_t operator()(const slang::parsing::Token& t) const noexcept
    {
        std::size_t h1 = std::hash<std::string_view>{}(t.rawText());
        std::size_t h2 = std::hash<slang::SourceRange>{}(t.range());
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};


template<>
struct std::hash<slang::syntax::SyntaxNode>
{
    std::size_t operator()(const slang::syntax::SyntaxNode& t) const noexcept
    {
        std::size_t h1 = std::hash<slang::parsing::Token>{}(t.getFirstToken());
        std::size_t h2 = std::hash<slang::parsing::Token>{}(t.getLastToken());
        return h1 ^ (h2 << 1); // or use boost::hash_combine
    }
};

template<>
struct std::hash<slang::syntax::ConstTokenOrSyntax>
{
    std::size_t operator()(const slang::syntax::ConstTokenOrSyntax& t) const noexcept
    {
        if(t.isNode())
            return std::hash<slang::syntax::SyntaxNode>{}(*(t.node()));
        else
            return std::hash<slang::parsing::Token >{}(t.token());
    }
};


#include "slang/syntax/SyntaxNode.h"
#include "slang/ast/Symbol.h"
#include "slang/text/SourceManager.h"
#include "slang/text/SourceLocation.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <optional>

#include "nlohmann/json.hpp"

/**
 * @brief Syntaxic sugar to get the source range of a ConstTokenOrSyntax
 * 
 * @param toknode The ConstTokenOrSyntax to handle. Does *not* support pointers.
 * Use `*toknode` in ordeer to handle them.
 */
#define CONST_TOKNODE_SR(toknode) ((toknode).isNode() ? (toknode).node()->sourceRange() : (toknode).token().range())


using json = nlohmann::json;

namespace slsp
{

    struct SyntaxNodePtrCmp
    {
        bool operator()(const slang::syntax::ConstTokenOrSyntax& lhs, const slang::syntax::ConstTokenOrSyntax& rhs) const;
    };

    typedef std::filesystem::path Index_FileID_t;
    typedef std::set<slang::syntax::ConstTokenOrSyntax, SyntaxNodePtrCmp> Index_LineContent_t; // Type representing a line in the index
    typedef std::unordered_map< unsigned int,  Index_LineContent_t> Index_FileContent_t; // Type representing the content of a single file.
    typedef std::unordered_map<Index_FileID_t, Index_FileContent_t >  SyntaxNodeIndex_t;



    class DiplomatIndex
    {
        protected:
            const slang::SourceManager* _sm;
            SyntaxNodeIndex_t _index;
            std::unordered_map<slang::SourceRange, const slang::ast::Symbol*> _definition_table;
            std::unordered_map<const slang::ast::Symbol*, std::unordered_set<slang::syntax::ConstTokenOrSyntax> > _reference_table;

            Index_FileID_t _index_from_filepath(const std::filesystem::path& filepath) const;   
            Index_FileID_t _index_from_symbol(const slang::ast::Symbol& symbol) const;   
            Index_FileID_t _ensure_index(const Index_FileID_t& idx);

        public:

            explicit DiplomatIndex(const slang::SourceManager* sm);

            Index_FileID_t ensure_file(const std::filesystem::path& fileref);
            void add_symbol(const slang::ast::Symbol& symbol);
            void add_symbol(const slang::ast::Symbol& symbol,  const slang::syntax::ConstTokenOrSyntax& anchor);
            void add_reference_to(const slang::ast::Symbol& symbol, const slang::syntax::ConstTokenOrSyntax& ref);
            void add_reference_to(const slang::ast::Symbol& symbol, const slang::syntax::ConstTokenOrSyntax& ref,const std::filesystem::path& reffile);
            
            bool is_registered(const slang::ast::Symbol& symbol) const;
            bool is_registered(const slang::ast::Symbol* symbol) const;
            bool is_registered(const slang::syntax::ConstTokenOrSyntax& node) const;
            bool is_registered(const std::filesystem::path& fileref) const;

            std::optional<slang::syntax::ConstTokenOrSyntax> get_syntax_from_position(const std::filesystem::path& file, unsigned int line, unsigned int character) const;
            std::optional<slang::syntax::ConstTokenOrSyntax> get_syntax_from_range(const slang::SourceRange& range) const;
            const slang::ast::Symbol* get_symbol_from_exact_range(const slang::SourceRange& range) const;

            slang::SourceRange get_definition(const slang::SourceRange& range) const;
            std::vector<slang::SourceRange> get_references(const slang::SourceRange& range) const;

            void cleanup();

            json dump();

    };
}