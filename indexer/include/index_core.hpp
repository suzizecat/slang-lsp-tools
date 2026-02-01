#pragma once 

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <ranges>

#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceManager.h>

#include "index_elements.hpp"
#include "index_scopetree_node.hpp"
#include "index_file.hpp"


#include "nlohmann/json.hpp"

/**
 * @brief The index namespace contains all the structures required 
 * to hold the source index used by diplomat.
 * 
 * As diplomat works with slang as a base, it will also include the visitors for the AST and CST.
 */
namespace diplomat::index
{
	
	class IndexCore
	{

	friend class IndexScopeVisitor;
	friend void to_json(nlohmann::json& j, const IndexCore& s);

	protected:
		std::unique_ptr<IndexScopeTreeNode> _root;
		std::map<std::filesystem::path, std::unique_ptr<IndexFile>> _files;

		/**
		 * @brief List of scopes that may be reused later on
		 * 
		 */
		std::unordered_map<uintptr_t, IndexScopeTreeNode* > _cached_scopes;

		//void _process_file_reference(slang::SourceManager* sm, const std::filesystem::path& fpath, IndexFile* f);

		void _cleanup_scope_symbols_step(IndexScopeTreeNode* scope);
		void _cleanup_scope_erase_step(IndexScopeTreeNode* scope);

	public:

		IndexScopeTreeNode* set_root_scope(const std::string name);
		inline IndexScopeTreeNode* get_root_scope(){return _root.get();};

		/**
		 * @brief Provide the bound status of the the root scope.
		 * 
		 * @return true if the root have been initialized
		 * @return false otherwise
		 */
		inline bool have_root_scope() const {return (bool)_root;};

		IndexFile* add_file(const std::filesystem::path& path);
		IndexFile* add_file(const std::string_view& path);

		IndexFile* get_file(const std::filesystem::path& path);
		const IndexFile* get_file(const std::filesystem::path& path) const;


		IndexSymbol* add_symbol(const std::string_view& name, const IndexRange& src_range, const std::string_view& kind = "");
		IndexSymbol* add_symbol(IndexSymbol* symb, const std::string_view& kind = "");

		/**
		* @brief Get a cached scope from its reference pointer value.
		* 
		* Intended usage is something like: 
		* \code{.cpp}
		* // IndexScopeTreeNode* parent;		
		* IndexScopeTreeNode* cached = get_cached_scope(node.getCanonicalBody);
		* if(cached != nullptr)
		* 	parent->add_subtree_child(cached);
		* else
		* 	process(node);
		* \endcode
		*
		* @param ref_ptr is the pointer returned by slang::ast::InstanceSymbol::getCanonicalBody()
		* @return IndexScopeTreeNode* the Index ScopeTree node related, if any, nullptr otherwise.
		*/
		IndexScopeTreeNode* get_cached_scope(const uintptr_t ref_ptr);

		/**
		 * @brief Store a scope as cached by a pointer.
		 *
		 * This aims to be used for slang::ast::InstanceSymbol::getCanonicalBody().
		 * 
		 * @sa get_cached_scope() 
		 *
		 * @param ref_ptr 
		 * @param scope 
		 */
		void cache_scope(const uintptr_t ref_ptr, IndexScopeTreeNode* const scope);


		inline nlohmann::json dump() const {return nlohmann::json(*this);} ;
		
		/**
		 * @brief Return a list of symbols found in each files
		 * 
		 * @return nlohmann::json JSON view of the index
		 */
		nlohmann::json dump_symbol_list() const;

	   inline auto get_indexed_files_paths() const { return std::views::keys(_files);} ;
	   inline auto get_indexed_files() const { return std::views::values(_files);} ;

		IndexScopeTreeNode* get_scope_by_position(const IndexLocation& pos);

		const IndexSymbol* get_symbol_by_position(const IndexLocation& pos) ;

		/**
		 * @brief If recorded in the index, proceed to invalidate a file.
		 * 
		 * If the file is not recorded, do nothing.
		 *
		 * @param file file path to invalidate
		 */
		void invalidate_file(const std::filesystem::path& file);

		/**
		 * @brief Get a scope by its fully qualified path (starting from the root)
		 * 
		 * @param path to evaluate
		 * @return IndexScope* if found, nullptr otherwise.
		 */
		IndexScopeTreeNode* lookup_scope(const std::string_view& path);


		/**
		 * @brief Perform cleanups operations
		 * 
		 */
		void cleanup();

		IndexCore() = default;
		~IndexCore() = default;
	};
	

	
	void to_json(nlohmann::json& j, const IndexCore& s);

}
