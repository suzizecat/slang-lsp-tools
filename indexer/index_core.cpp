#include "index_core.hpp"
#include "index_reference_visitor.hpp"

#include <spdlog/spdlog.h>

namespace diplomat::index {
	IndexScope* IndexCore::set_root_scope(const std::string name)
	{
		_root.reset(new IndexScope(name,false));
		return _root.get();
	}

	IndexFile *IndexCore::get_file(const std::filesystem::path& path)
	{
		std::filesystem::path lookup_path = std::filesystem::weakly_canonical(path);
		if(! _files.contains(lookup_path))
			_files.emplace(lookup_path,new IndexFile(lookup_path));

		return _files.at(lookup_path).get();
	}

	IndexFile *IndexCore::get_file(const std::string_view& path)
	{
		return get_file(std::filesystem::path(path));
	}

	IndexSymbol *IndexCore::add_symbol(const std::string_view& name, const IndexRange& src_range)
	{
		IndexFile* f = get_file(src_range.start.file);
		IndexSymbol* s = f->add_symbol(name,src_range);
		return s;
	}

	// void IndexCore::process_references()
	// {
	// 	for(auto& [path, file] : _files)
	// 	{
	// 		{
	// 			spdlog::info("Processing references for {}",path.generic_string());
	// 			// Ensure that the buffer is destroyed each time
	// 			slang::SourceManager curr_sm;
	// 			auto stx = slang::syntax::SyntaxTree::fromFile(path.generic_string(),curr_sm);
	// 			if(stx.has_value())
	// 			{
	// 				ReferenceVisitor ref_visitor(&curr_sm,this);
	// 				stx.value()->root().visit(ref_visitor);
	// 			}
	// 		}
	// 	}
	// }


	void to_json(nlohmann::json &j, const IndexCore &s)
	{

		j = nlohmann::json{
			{"hier",s._root},
			{"files",s._files}};
	}
}