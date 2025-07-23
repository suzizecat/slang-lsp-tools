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

	const IndexFile* IndexCore::get_file(const std::filesystem::path& path) const
	{
		std::filesystem::path lookup_path = std::filesystem::weakly_canonical(path);
		if(! _files.contains(lookup_path))
			return nullptr;

		return _files.at(lookup_path).get();
	}

	IndexSymbol *IndexCore::add_symbol(const std::string_view& name, const IndexRange& src_range, const std::string_view& kind)
	{
		IndexFile* f = add_file(src_range.start.file);
		IndexSymbol* s = f->add_symbol(name,src_range,kind);
		return s;
	}

	nlohmann::json IndexCore::dump_symbol_list() const
	{
		using namespace nlohmann; 
		json ret;
		for(const auto& [path, idx_file] : _files )
		{
			json file_content = json::array();
			for(const auto& [loc, ref] : idx_file->get_references())
			{
				file_content.push_back({{"loc",loc},{"name",ref.key->get_name()}});
			}
			ret[path.generic_string()] = file_content;
		}

		return ret;
	}

	IndexScope* IndexCore::get_scope_by_position(const IndexLocation& pos)
	{
		if(! _files.contains(pos.file))
			return nullptr;
		else
			return _files.at(pos.file)->lookup_scope_by_location(pos);
	}

	const IndexSymbol* IndexCore::get_symbol_by_position(const IndexLocation& pos)
	{
		IndexFile* ref_file = get_file(pos.file);
		if(! ref_file)
			return nullptr;
		return ref_file->lookup_symbol_by_location(pos);
	}

	IndexScope* IndexCore::lookup_scope(const std::string_view& path)
	{
		return _root->resolve_scope(path);
	}

	void to_json(nlohmann::json &j, const IndexCore &s)
	{

		j = nlohmann::json{
			{"hier",s._root},
			{"files",s._files}};
	}
}