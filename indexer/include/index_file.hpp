#pragma once

#include <filesystem>
#include "index_scope.hpp"
#include "index_scopetree_node.hpp"
#include "index_symbols.hpp"
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
        ReferenceRecord(const IndexRange& loc, IndexSymbol* const& symb, bool is_definition = false) : loc(loc), key(symb), is_definition(is_definition) {};
        IndexRange loc;
        IndexSymbol* key;
        bool is_definition;
    };

    class IndexFile
    {

        friend void to_json(nlohmann::json& j, const IndexFile& s);
    protected:
        std::filesystem::path _filepath;

        // This may contain the syntax tree used to define the file
        std::optional<const slang::syntax::SyntaxNode*> _syntax_root; 

        // Used for fast lookup of scopes
        std::unordered_map<std::string, IndexScopeTreeNode*> _scopes;
        std::map<IndexLocation, IndexSymbol*> _declarations;
        
        /**
         * @brief This variable references all ref records of the file, sorted by location (start of the token).
         * 
         * This variable is used to perform geographic lookup of references (in exemple to check if a
         * reference is under the cursor). This may be done by using <tt>upper_bound -1</tt> and checking if the
         * reference range actually matches the looked up location
         */
        std::map<IndexLocation, ReferenceRecord> _references;

        /**
         * @brief This variable holds the scopes locations for fast lookup.
         * 
         *
         * @note In order to lookup a scope, use <tt>upper_bound -1</tt> and check the range.
         *
         */
        std::map<IndexLocation, IndexScopeTreeNode* > _scopes_locations;

        // /**
        //  * @brief This container holds the list of additional scopes that should be looked up
        //  * for reference resolution.
        //  * 
        //  * If the IndexScope* is nullptr, the system should try to resolve it, as it is lazily
        //  * built through the AST.
        //  * 
        //  * The key is the path from the root element of the AST of the scope.
        //  * 
        //  */
        // std::map<std::string, IndexScope*> _additional_lookup_scopes;

        #ifdef DIPLOMAT_DEBUG
            std::vector<std::string> _failed_references;
        #endif


    public:
        IndexFile(const std::filesystem::path& path);
        ~IndexFile() = default;

        /**
         * @brief Record a (new) symbol with the provided name and definition location in the current file
         * 
         * @param name 
         * @param location 
         * @param kind 
         * @return IndexSymbol* 
         */
        IndexSymbol* add_symbol(IndexSymbol* symb);
        // IndexSymbol* add_symbol(const std::string_view& name, const IndexRange& location, const std::string_view& kind = {});
        void register_scope(IndexScopeTreeNode* _scope); 
        IndexScopeTreeNode* lookup_scope_by_range(const IndexRange& loc);
        IndexScopeTreeNode* lookup_scope_by_exact_range(const IndexRange& loc);
        IndexScopeTreeNode* lookup_scope_by_location(const IndexLocation& loc);

        IndexSymbol* lookup_symbol_by_location(const IndexLocation& loc);

        void add_reference(IndexSymbol* symb, const IndexRange& range, bool is_definition = false );
        inline const std::map<IndexLocation, ReferenceRecord>& get_references() const {return _references;};
        inline auto get_symbols() const {return std::views::values(_declarations);};

        inline void set_syntax_root(const slang::syntax::SyntaxNode* node ) {_syntax_root = node;};
        inline const slang::syntax::SyntaxNode* get_syntax_root() const {return _syntax_root.value_or(nullptr);};

        inline const std::filesystem::path& get_path() const {return _filepath;} ;

        // void record_additionnal_lookup_scope(const std::string& path, IndexScope* target = nullptr);
        // void invalidate_additionnal_lookup_scope(const std::string& path);

        //inline const std::map<std::string, IndexScope*>* get_additionnal_scopes() const {return &_additional_lookup_scopes;} ;


        #ifdef DIPLOMAT_DEBUG
        inline void _add_failed_ref(const std::string & ref_text) {_failed_references.push_back(ref_text);};
        inline std::size_t _get_nb_failed_refs() const {return _failed_references.size();};
        #else
        constexpr  void _add_failed_ref(const std::string & ref_text) {};
        constexpr  std::size_t _get_nb_failed_refs() const {return 0;};
        #endif
    };

    void to_json(nlohmann::json& j, const IndexFile& s);
} // namespace diplomat:index     
