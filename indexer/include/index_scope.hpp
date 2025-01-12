/**
 * @file index_scope.hpp   
 * @author Julien FAUCHER
 * @brief This file describes the object that models a "scope" in the index
 * @version 0.1
 * @date 2024-12-21
 * 
 * @copyright 2024 Julien FAUCHER 
 * MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */
#pragma once

#include "nlohmann/json.hpp"
#include "index_elements.hpp"
#include "index_symbols.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <filesystem>

namespace diplomat::index
{
    
    class IndexScope
    {

        friend void to_json(nlohmann::json& j, const IndexScope& s);
	    friend void from_json(const nlohmann::json& j, IndexScope& s); 

    protected:
        std::string _name;
        IndexScope* _parent;
        std::unordered_map<std::string, std::unique_ptr<IndexScope> > _children;
        
        /**
         * @brief When two sub scope are refering to the same piece of code,
         * the duplicate will be added to this table instead of creating a full
         * scope, thus easing the process of lookups 
         * 
         */
        std::unordered_map<std::string, IndexScope*> _child_aliases;

        /**
         * @brief Range that cover the scope declaration and content
         */
        std::optional<IndexRange> _source_range;

        // Symbols should be created as a file level, then forwarded to the scope for registering.
        std::unordered_map<std::string, IndexSymbol*> _content;

        /**
         * @brief This variable represents the fact that the scope impacts the design hierarchy
         * 
         */
        bool _is_virtual;

        //bool _can_access_parent;

        size_t _hash_value;

        size_t _unnamed_count;
        bool _anonymous;

        #ifdef DIPLOMAT_DEBUG
        std::string_view _kind;
        #endif

        /**
         * @brief Build the set of non-virtual children for the `get_concrete_children` function.
         * 
         * This function recursively go into every virtual children until non-virtual scopes are found.
         * Such concrete scopes are then added to the return holder.
         * @param ret_holder The set used to collect all captured scopes
         * @param is_root if true, prevent capturing the scope from wich the lookup is originating.
         * Should be false any other time.
         * 
         * @sa IndexScope::get_concrete_children()
         */
        void _build_concrete_children(std::set<IndexScope*>& ret_holder, bool is_root = true);


    public:
        IndexScope() = default;
        IndexScope(std::string name, bool isvirtual = false, bool anonymous = false);
        ~IndexScope() = default;

        inline void set_kind(const std::string_view& kind) {
			#ifdef DIPLOMAT_DEBUG
			_kind = kind;
			#endif
		};

        /**
         * @brief Construct a sub-scope and return the shared pointer to it.
         * 
         * @param name the name of the created sub-scope
         * @param is_virtual sets the "virtual" flag of the new scope
         * @return std::shared_ptr<IndexScope> the shared pointer to manipulate said scope.
         */
        IndexScope* add_child(const std::string& name, const bool is_virtual = false);

        /**
         * @brief Add an alias for the specified child, then return the actual scope if OK,
         * nullptr otherwise.
         * 
         * @param ref Reference child
         * @param alias Name of the alias
         * @return IndexScope* reference child if the alias is in place, nullptr otherwise
         */
        IndexScope* add_child_alias(const std::string& ref, const std::string& alias);

        /** 
         * @brief Add a symbol to the scope.
         * 
         * @param symbol pointer to the already existing symbol to ass
         */
        void add_symbol(IndexSymbol* symbol);

        /**
         * @brief Lookup a symbol by name that should be available in this scope
         * 
         * @param name Name to lookup
         * @param strict If strict is false, recursively lookup in virtual parent scopes until the symbol is found
         * @return IndexSymbol* pointer to the symbol if found, nullptr otherwise
         */
        IndexSymbol* lookup_symbol(const std::string& name, bool strict = false);

        /**
         * @brief Retrieve a symbol based upon its fully qualified name, relative to the current scope.
         * 
         * @param path Hierarchical path to the symbol to lookup
         * @return IndexSymbol* the found symbol if any. nullptr otherwise.
         */
        IndexSymbol* resolve_symbol(const std::string_view& path);

        /**
         * @brief Retrieve a sub scope based upon its fully qualified name, relative to the 
         * current scope.
         * 
         * @param path relative to the current scope
         * @return IndexScope* if found, nullptr otherwise. 
         */
        IndexScope* resolve_scope(const std::string_view& path);


        /**
         * @brief Get the visible symbols object from the current scope.
         * This represent all symbols declared here and all symbols declared in parents.
         * @return std::vector<const IndexSymbol*> the set of found symbols.
         */
        std::vector<const IndexSymbol*> get_visible_symbols() const;

        /**
         * @brief Get the scope for position object
         * 
         * The scope should be the most specific scope for a given location
         * 
         * @param loc location to lookup for
         * @param deep performs a deep search (the result may not be in the same file).
         * @warning Deep resolution may be computing intensive.
         * @return IndexScope* pointer to the scope if valid, nullptr otherwise.
         */
        IndexScope* get_scope_for_location(const IndexLocation& loc, bool deep = false);

        /**
         * @brief Get the most specific scope including the given range
         * 
         * @param loc source range to lookup
         * @param deep performs a deep search: always enter children scopes (regardless of their location)
         * in order to lookup range across files.
         * @warning Deep resolution may be computing intensive.
         * @return IndexScope* 
         */
        IndexScope* get_scope_for_range(const IndexRange& loc, bool deep = false);
        
        /**
         * @brief Get the child for exact range object.
         * This function may be used to search for a **direct child** of the current scope
         * wich would exactly cover the provided range.
         * 
         * It is useful for duplication detection.
         * 
         * @param loc Range to lookup
         * @return IndexScope* found scope if any, nullptr otherwise.
         * @sa IndexScope::get_scope_for_range
         */
        IndexScope* get_child_by_exact_range(const IndexRange& loc);

        /**
         * @brief Direct lookup a child scope by its name.
         * 
         * @param name to lookup
         * @return IndexScope* if the scope is found, nullptr otherwise
         */
        IndexScope* get_scope_by_name(const std::string_view& name);

        std::string get_full_path() const;
        std::string get_concrete_path() const;

        std::set<IndexScope*> get_concrete_children();

        size_t compute_hash_value();
        inline size_t get_hash_value() const { return _hash_value; };

        inline bool get_parent_access() const { return _is_virtual;} ;
        inline const std::string& get_name() const {return _name;};

        inline void set_source(const IndexRange& range) {_source_range = range;};
        inline const std::optional<IndexRange>& get_source_range() const { return _source_range;};
        inline bool is_anonymous() const { return _anonymous;};
    };  


	void to_json(nlohmann::json& j, const IndexScope& s);
	//void from_json(const nlohmann::json& j, IndexScope& s); 
	

    
} // namespace diplomat::index


