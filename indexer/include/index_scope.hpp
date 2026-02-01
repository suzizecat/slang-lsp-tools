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
#include <ranges>
namespace diplomat::index
{
    
    class IndexScope
    {

        friend void to_json(nlohmann::json& j, const IndexScope& s);
	    friend void from_json(const nlohmann::json& j, IndexScope& s); 

    protected:
        
        /**
         * @brief Range that cover the scope declaration and content
         */
        std::optional<IndexRange> _source_range;

        /**
         * @brief List of symbols declared in this scope
         * 
         */
        std::unordered_map<std::string, std::unique_ptr<IndexSymbol>> _content;

        size_t _hash_value;

        #ifdef DIPLOMAT_DEBUG
        std::string_view _kind;
        #endif

    public:
        IndexScope() = default;
        //IndexScope(std::string name, bool isvirtual = false, bool anonymous = false);
        ~IndexScope() = default;

        inline void set_kind(const std::string_view& kind) {
			#ifdef DIPLOMAT_DEBUG
			_kind = kind;
			#endif
		};


        /** 
         * @brief Add a symbol to the scope.
         * 
         * @param symbol pointer to the already existing symbol to add.
         * the scope will take ownership of the symbol
         */
        IndexSymbol* add_symbol(IndexSymbol* symb );

        /** 
         * @brief Add a symbol to the scope.
         * 
         * @param symbol pointer to the already existing symbol to add.
         * the scope will take ownership of the symbol
         */
        IndexSymbol* add_symbol(std::unique_ptr<IndexSymbol> symb );

        /**
         * @brief Lookup a symbol by name that should be available in this scope
         * 
         * @param name Name to lookup
         * @param strict If strict is false, recursively lookup in virtual parent scopes until the symbol is found
         * @return IndexSymbol* pointer to the symbol if found, nullptr otherwise
         */
        IndexSymbol* get_symbol(const std::string& name);

        /**
         * @brief Get the visible symbols object from the current scope.
         * This represent all symbols declared here and all symbols declared in parents.
         * @return std::vector<const IndexSymbol*> the set of found symbols.
         */
        std::vector<const IndexSymbol*> get_visible_symbols() const;

 

        inline void set_source(const IndexRange& range) {_source_range = range;};
        inline const std::optional<IndexRange>& get_source_range() const { return _source_range;};
        inline const auto get_symbols() const {return std::views::values(_content);};

    };  


	void to_json(nlohmann::json& j, const IndexScope& s);
	//void from_json(const nlohmann::json& j, IndexScope& s); 
	

    
} // namespace diplomat::index


