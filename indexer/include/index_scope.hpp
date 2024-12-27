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


    public:
        IndexScope() = default;
        IndexScope(std::string name, bool isvirtual = false);
        ~IndexScope() = default;

        /**
         * @brief Construct a sub-scope and return the shared pointer to it.
         * 
         * @param name the name of the created sub-scope
         * @param is_virtual sets the "virtual" flag of the new scope
         * @return std::shared_ptr<IndexScope> the shared pointer to manipulate said scope.
         */
        IndexScope* add_child(const std::string& name, const bool is_virtual = false);

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
         * @param strict If strict is false, recursively lookup (if possible) in parent scopes until the symbol is found
         * @return IndexSymbol* pointer to the symbol if found, nullptr otherwise
         */
        IndexSymbol* lookup_symbol(const std::string& name, bool strict = false);

        /**
         * @brief Get the scope for position object
         * 
         * The scope should be the most specific scope for a given location
         * 
         * @param loc location to lookup for
         * @param deep performs a deep search (the result may not be in the same )
         * @return IndexScope* pointer to the scope if valid, nullptr otherwise.
         */
        IndexScope* get_scope_for_location(const IndexLocation& loc, bool deep = false);

        IndexScope* get_scope_for_range(const IndexRange& loc, bool deep = false);

        std::string get_full_path() const;
        std::string get_concrete_path() const;

        size_t compute_hash_value();
        inline size_t get_hash_value() const { return _hash_value; };

        inline bool get_parent_access() const { return _is_virtual;} ;
        inline const std::string& get_name() const {return _name;};

        inline void set_source(const IndexRange& range) {_source_range = range;};
    };  


	void to_json(nlohmann::json& j, const IndexScope& s);
	//void from_json(const nlohmann::json& j, IndexScope& s); 
	

    
} // namespace diplomat::index


