#pragma once

#include <filesystem>
#include "index_scope.hpp"
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
#include <ranges>

#include <slang/syntax/SyntaxNode.h>
namespace diplomat::index      
{

    struct ReferenceRecord
    {
        ReferenceRecord(const IndexRange& loc, IndexSymbol* const& symb) : loc(loc), key(symb) {};
        IndexRange loc;
        IndexSymbol* key;
    };

    class IndexFile
    {

        friend void to_json(nlohmann::json& j, const IndexFile& s);
    protected:
        std::filesystem::path _filepath;

        // This may contain the syntax tree used to define the file
        std::optional<const slang::syntax::SyntaxNode*> _syntax_root; 

        // Used for fast lookup of scopes
        std::unordered_map<std::string, IndexScope*> _scopes;
        std::unordered_map<IndexRange, std::unique_ptr<IndexSymbol>> _declarations;
        
        // References are located by start of location.
        // In order to lookup a reference, use upper_bound -1 and check the range
        std::map<IndexLocation, ReferenceRecord> _references;

    public:
        IndexFile(const std::filesystem::path& path);
        ~IndexFile() = default;

        IndexSymbol* add_symbol(const std::string_view& name, const IndexRange& location);
        void register_scope(IndexScope* _scope); 
        IndexScope* lookup_scope_by_range(const IndexRange& loc);
        IndexScope* lookup_scope_by_location(const IndexLocation& loc);

        IndexSymbol* lookup_symbol_by_location(const IndexLocation& loc);

        void add_reference(IndexSymbol* symb, const IndexRange& range );
        inline const std::map<IndexLocation, ReferenceRecord>& get_references() const {return _references;};
        inline auto get_symbols() const {return std::views::values(_declarations);};

        inline void set_syntax_root(const slang::syntax::SyntaxNode* node ) {_syntax_root = node;};
        inline const slang::syntax::SyntaxNode* get_syntax_root() const {return _syntax_root.value_or(nullptr);};

        inline const std::filesystem::path& get_path() const {return _filepath;} ;
    };

    void to_json(nlohmann::json& j, const IndexFile& s);
} // namespace diplomat:index     
