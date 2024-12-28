#include "index_core.hpp"
#include "index_reference_visitor.hpp"

#include <spdlog/spdlog.h>

namespace diplomat::index {
	IndexScope* IndexCore::set_root_scope(const std::string name)
	{
		_root.reset(new IndexScope(name,false));
		return _root.get();
	}

	IndexFile *IndexCore::add_file(const std::filesystem::path& path)
	{
		std::filesystem::path lookup_path = std::filesystem::weakly_canonical(path);
		if(! _files.contains(lookup_path))
			_files.emplace(lookup_path,new IndexFile(lookup_path));

		return _files.at(lookup_path).get();
	}

	IndexFile *IndexCore::add_file(const std::string_view& path)
	{
		return add_file(std::filesystem::path(path));
	}

	IndexFile *IndexCore::get_file(const std::filesystem::path& path)
	{
		std::filesystem::path lookup_path = std::filesystem::weakly_canonical(path);
		if(! _files.contains(lookup_path))
			return nullptr;

		return _files.at(lookup_path).get();
	}

	IndexSymbol *IndexCore::add_symbol(const std::string_view& name, const IndexRange& src_range)
	{
		IndexFile* f = add_file(src_range.start.file);
		IndexSymbol* s = f->add_symbol(name,src_range);
		return s;
	}

	IndexScope* IndexCore::get_scope_by_position(const IndexLocation& pos)
	{
		if(! _files.contains(pos.file))
			return nullptr;
		else
			return _files.at(pos.file)->lookup_scope_by_location(pos);
	}


	void to_json(nlohmann::json &j, const IndexCore &s)
	{

		j = nlohmann::json{
			{"hier",s._root},
			{"files",s._files}};
	}
}