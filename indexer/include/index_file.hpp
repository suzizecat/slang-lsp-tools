#pragma once

#include <filesystem>
#include "index_scope.hpp"
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>
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

        void add_reference(IndexSymbol* symb, IndexRange& range );
    };

    void to_json(nlohmann::json& j, const IndexFile& s);
} // namespace diplomat:index     
