#pragma once

#include "slang/syntax/SyntaxNode.h"
#include "slang/ast/Symbol.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <optional>


namespace slsp
{

    struct SyntaxNodePtrCmp
    {
        bool operator()(const slang::syntax::SyntaxNode* lhs, const slang::syntax::SyntaxNode* rhs) const;
    };

    typedef std::filesystem::path Index_FileID_t;
    typedef std::set< const slang::syntax::SyntaxNode*, SyntaxNodePtrCmp> Index_LineContent_t; // Type representing a line in the index
    typedef std::unordered_map< unsigned int,  Index_LineContent_t> Index_FileContent_t; // Type representing the content of a single file.
    typedef std::unordered_map<Index_FileID_t, Index_FileContent_t >  SyntaxNodeIndex_t;



    class DiplomatIndex
    {
        protected:

        

            SyntaxNodeIndex_t _index;
            std::unordered_map<const slang::syntax::SyntaxNode*, const slang::ast::Symbol*> _definition_table;
            std::unordered_map<const slang::ast::Symbol*, std::unordered_set<const slang::syntax::SyntaxNode*> > _reference_table;

            Index_FileID_t _index_from_filepath(const std::filesystem::path& filepath) const;   
            Index_FileID_t _index_from_symbol(const slang::ast::Symbol& symbol) const;   
            Index_FileID_t _ensure_index(const Index_FileID_t& idx);

        public:
            Index_FileID_t ensure_file(const std::filesystem::path& fileref);
            void add_symbol(const slang::ast::Symbol& symbol);
            void add_reference_to(const slang::ast::Symbol& symbol, const slang::syntax::SyntaxNode& ref,const std::filesystem::path& reffile);
            
            bool is_registered(const slang::ast::Symbol& symbol) const;
            bool is_registered(const slang::syntax::SyntaxNode& node) const;
            void cleanup();
        
    };
}