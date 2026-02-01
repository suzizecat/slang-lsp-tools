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
#include "index_scope.hpp"
namespace diplomat::index
{
	
	/**
	 * @brief Structural node for the Index Scope Tree.
	 *
	 * This class holds the structural informations about a scope.
	 * It also contains all utilities for looking up scope as well as the scope name.
	 * 
	 * A node without parent may be used as the root of a scope tree.
	 * 
	 * @note All information about the content of the scope (symbols and so on) will be 
	 * delegated to a ::IndexScope object that will be referenced by the ::IndexScopeTreeNode.
	 * Hence it will be possible to duplicate all symbols from a scope without having to compute all new symbols.
	 */
	class IndexScopeTreeNode
	{

		 friend void to_json(nlohmann::json& j, const IndexScopeTreeNode& s);
		// friend void from_json(const nlohmann::json& j, IndexScope& s); 

	protected:
		/** Name of the described scope */
		std::string _name;
		/** 
		* @brief Actual data informations held by the scope.
		*
		* It means all information not directly related to the structure of the scope tree
		* such as symbols (mainly)
		*
		* @note This data should be used multiple times if one scope is used several times.
		*/
		std::shared_ptr<IndexScope> _data;

		/**
		 * @brief Childs scopes, referenced by their names for quick lookup when exploring the hierarchy.
		 * 
		 * Both virtual and concrete children will be stored here. 
		 * 
		 * Those children are to be considered on a structural level, and are therefore always unique.
		 * They may, however, point toward a unique diplomat::index::IndexScope* for their #_data attribute.
		 */
		std::unordered_map<std::string, std::unique_ptr<IndexScopeTreeNode> > _children;

		/**
		 * Reference to the parent (structural) node.
		 */
		IndexScopeTreeNode* _parent;
		

		/**
		 * @brief Scope 'virtual' attribute
		 * 
		 * A 'virtual' scope is a scope that is able to implicitely see the elements of its parents
		 * while a 'concrete' scope has no knowledge of its parents.
		 *
		 * Typically, a module instance is a concrete scope, while a generate bloc (or any kind of begin .. end block)
		 * is a virtual scope.
		 *
		 * Virtual scopes only have access to their *parents*, not their siblings. 
		 */
		bool _is_virtual;

		// size_t _hash_value;
		
		/** 
		* @brief Counter for the number of anonymous childs, used to generate default unnamed_xxx names 
		*
		* @sa #_get_unnamed_id()
		*/
		size_t _unnamed_count;


		/**
		 * @brief Tracks the valid state of the scope tree node.
		 * 
		 * An 'invalid' scope means that the file containing the scope has been modified
		 * and needs to be re-analyzed.
		 * Therefore, the scope may have changed or could have been deleted altogether. 
		 */
		bool _valid;


		/**
		 * @brief Generate an unnamedX id based upon the unnamed count and return it.
		 * This will increate the unnamed_count.
		 * 
		 * @return std::string the new identifier.
		 */
		std::string _get_unnamed_id();

		void _attach_child(IndexScopeTreeNode* new_child);
		IndexScopeTreeNode* _attach_child(std::unique_ptr<IndexScopeTreeNode> new_child);

	public:
		IndexScopeTreeNode() = delete;
		/**
		 * @brief Construct a new Index Scope Tree Node object referencing an already existing IndexScope data object
		 * 
		 * @param data The data object to reference (create a new one if \c nullptr is passed)
		 * @param name The (optional) name for the new node
		 * @param parent The (potential) parent
		 * @param isvirtual Set to true if the node represents a virtual node
		 *
		 * @throw index_exception if the parent already have a child with the same name.
		 * Anonymous node can be made by not specifying \p name (it is possible to use <tt>{}</tt> as a non-specified value)
		 */
		IndexScopeTreeNode(std::shared_ptr<IndexScope> data = nullptr, std::optional<std::string> name = {},  IndexScopeTreeNode* parent = nullptr,  bool isvirtual = false);
		//IndexScopeTreeNode(std::optional<std::string> name = {} ,  IndexScopeTreeNode* parent = nullptr,  bool isvirtual = false);
		// IndexScopeTreeNode(std::string name, IndexScopeTreeNode* parent = nullptr,  bool isvirtual = false);
		~IndexScopeTreeNode() = default;

		// inline void set_kind(const std::string_view& kind) {
		// 	#ifdef DIPLOMAT_DEBUG
		// 	_kind = kind;
		// 	#endif
		// };

		/**
		 * @brief Construct or return a children scope with the provided data attached.
		 * 
		 * @param name the name of the created sub-scope
		 * @param data is a pointer toward an existing node data object. 
		 * @param is_virtual sets the "virtual" flag of the new scope
		 * @return IndexScopeTreeNode* the pointer to manipulate said scope.
		 */
		 IndexScopeTreeNode* add_child(const std::string& name, std::shared_ptr<IndexScope> data = nullptr , const bool is_virtual = false);
		 
		 /**
		  * @brief Construct an anonymous children scope with the provided data attached.
		  * 
		  * @param data Data to link to the child
		  * @param is_virtual Virtual attribute of the node
		  * @return IndexScopeTreeNode* The created (and ready) node
		  */
		 IndexScopeTreeNode* add_anon_child(std::shared_ptr<IndexScope>  data = nullptr, const bool is_virtual = false);


		/**
		 * @brief Adds a full scope subtree as a child
		 *
		 * In order to accomodate for the caching system of Slang, it may be required to insert a 
		 * child that is actually a fully fledged scope subtree.
		 * 
		 * In this case, the direct child name may be overriden, but all subsequent information will 
		 * be duplicated.
		 * This will also perform a duplication of the subtree.
		 * The #_data field, however, will point toward the same IndexScope as the originals elements
		 * in order to avoid symbol duplication. 
		 * 
		 * @param reference reference child to duplicate
		 * @param name new name for the new child
		 * @return IndexScopeTreeNode* created new child
		 */
		IndexScopeTreeNode* add_subtree_child(const IndexScopeTreeNode* reference, const std::string& name);
		/** 
		 * @brief Add a symbol to the scope.
		 * 
		 * @param symbol pointer to the already existing symbol to add
		 * @returns  the pointer to the actually created symbol, for convenience
		 */
		IndexSymbol* add_symbol(IndexSymbol* symbol);

		IndexSymbol* add_symbol(std::unique_ptr<IndexSymbol> symbol);

		/**
		 * @brief Lookup a symbol by name that should be available in this scope
		 * 
		 * @param name Name to lookup
		 * @param strict If strict is false, recursively lookup in virtual parent scopes until the symbol is found
		 * @return IndexSymbol* pointer to the symbol if found, nullptr otherwise
		 */
		IndexSymbol* lookup_symbol(const std::string_view& name, bool strict = false);

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
		 * @return IndexScopeTreeNode* if found, nullptr otherwise. 
		 */
		IndexScopeTreeNode* resolve_scope(const std::string_view& path);


		/**
		 * @brief Get the visible symbols object from the current scope.
		 * This represent all symbols declared here and all symbols declared in parents.
		 * @return std::vector<const IndexSymbol*> the set of found symbols.
		 */
		std::vector<const IndexSymbol*> get_visible_symbols(std::optional<IndexLocation> exact = {}) const;

		/**
		 * @brief Get the scope for position object
		 * 
		 * The scope should be the most specific scope for a given location
		 * 
		 * @param loc location to lookup for
		 * @param deep performs a deep search (the result may not be in the same file).
		 * @warning Deep resolution may be computing intensive.
		 * @return IndexScopeTreeNode* pointer to the scope if valid, nullptr otherwise.
		 */
		IndexScopeTreeNode* get_scope_for_location(const IndexLocation& loc, bool deep = false);

		/**
		 * @brief Get the most specific scope including the given range
		 * 
		 * @param loc source range to lookup
		 * @param deep performs a deep search: always enter children scopes (regardless of their location)
		 * in order to lookup range across files.
		 * @warning Deep resolution may be computing intensive.
		 * @return IndexScopeTreeNode* 
		 */
		IndexScopeTreeNode* get_scope_for_range(const IndexRange& loc, bool deep = false);
		
		/**
		 * @brief Get the child for exact range object.
		 * This function may be used to search for a **direct chilstd::set<IndexScopeTreeNode*>d** of the current scope
		 * wich would exactly cover the provided range.
		 * 
		 * It is useful for duplication detection.
		 * 
		 * @param loc Range to lookup
		 * @return IndexScopeTreeNode* found scope if any, nullptr otherwise.
		 * @sa IndexScopeTreeNode::get_scope_for_range
		 */
		IndexScopeTreeNode* get_child_by_exact_range(const IndexRange& loc);

		/**
		 * @brief Direct lookup a child scope by its name.
		 * 
		 * @param name to lookup
		 * @return IndexScopeTreeNode* if the scope is found, nullptr otherwise
		 */
		IndexScopeTreeNode* get_scope_by_name(const std::string_view& name);

		std::string get_full_path() const;
		std::string get_concrete_path() const;

		IndexScopeTreeNode* get_root();
		const IndexScopeTreeNode* get_root() const;

		inline auto get_children() const {return std::views::values(_children);};

		// size_t compute_hash_value();
		// inline size_t get_hash_value() const { return _hash_value; };

		inline bool have_parent_access() const { return _is_virtual;} ;
		inline const std::string& get_name() const {return _name;};

		inline void set_source(const IndexRange& range) {_data->set_source(range);};
		inline const std::optional<IndexRange>& get_source_range() const { return _data->get_source_range();};
		// inline bool is_anonymous() const { return _anonymous;};
		inline bool is_virtual() const { return _is_virtual;};

		inline std::shared_ptr<const IndexScope> data() const {return _data;} ;
		inline std::shared_ptr<IndexScope> data() {return _data;} ;

		inline void invalidate() {_valid = false;}; 
		inline void validate() {_valid = true;};
		inline bool is_valid() const {return _valid;};

		/**
		 * @brief The cleanup step will recusrively remove all childs that are still invalid.
		 * 
		 * All references will be expected to be deleted beforehand.
		 * 
		 */
		void cleanup();
	};  


	void to_json(nlohmann::json& j, const IndexScopeTreeNode& s);
	//void from_json(const nlohmann::json& j, IndexScopeTreeNode& s); 
	

	
} // namespace diplomat::index


