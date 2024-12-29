#pragma once 

#include <map>
#include <memory>
#include <string>
#include <ranges>

#include <slang/syntax/SyntaxTree.h>
#include <slang/text/SourceManager.h>

#include "index_elements.hpp"
#include "index_scope.hpp"
#include "index_file.hpp"


#include "nlohmann/json.hpp"

namespace diplomat::index
{
	
	class IndexCore
	{

	friend class IndexScopeVisitor;
	friend void to_json(nlohmann::json& j, const IndexCore& s);

	protected:
		std::unique_ptr<IndexScope> _root;
		std::map<std::filesystem::path, std::unique_ptr<IndexFile>> _files;

		//void _process_file_reference(slang::SourceManager* sm, const std::filesystem::path& fpath, IndexFile* f);
	public:

		IndexScope* set_root_scope(const std::string name);
		inline IndexScope* get_root_scope(){return _root.get();};

		IndexFile* add_file(const std::filesystem::path& path);
		IndexFile* add_file(const std::string_view& path);

		IndexFile* get_file(const std::filesystem::path& path);
		const IndexFile* get_file(const std::filesystem::path& path) const;

		IndexSymbol* add_symbol(const std::string_view& name, const IndexRange& src_range);

		inline nlohmann::json dump() const {return nlohmann::json(*this);} ;


	   inline auto get_indexed_files_paths() const { return std::views::keys(_files);} ;
	   inline auto get_indexed_files() const { return std::views::values(_files);} ;

		IndexScope* get_scope_by_position(const IndexLocation& pos);

		const IndexSymbol* get_symbol_by_position(const IndexLocation& pos) ;

		IndexCore() = default;
		~IndexCore() = default;
	};
	

	
	void to_json(nlohmann::json& j, const IndexCore& s);

}