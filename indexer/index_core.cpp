#include "index_core.hpp"
#include "index_exceptions.hpp"
#include "index_reference_visitor.hpp"
#include "index_scopetree_node.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>

namespace diplomat::index {
	IndexScopeTreeNode* IndexCore::set_root_scope(const std::string name)
	{
		_root.reset(new IndexScopeTreeNode(nullptr, name,nullptr,false));
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

	IndexSymbol* IndexCore::add_symbol(const std::string_view& name, const IndexRange& src_range, const std::string_view& kind)
	{
		IndexFile* f = add_file(src_range.start.file);
		IndexScopeTreeNode* scope = f->lookup_scope_by_range(src_range);
		if(! scope)
			return nullptr;

		
		IndexSymbol* s = scope->add_symbol(std::make_unique<IndexSymbol>(name, src_range));
		f->add_symbol(s);
		return s;
	}

	IndexSymbol* IndexCore::add_symbol(IndexSymbol* symb, const std::string_view& kind)
	{
		if(symb->get_source())
		{
			IndexFile* f = add_file(symb->get_source_location()->file);
			f->add_symbol(symb);
		}
		else {
			throw index_exception("Tried to register a symbol without location");
		}
		return symb;
	}

	IndexScopeTreeNode* IndexCore::get_cached_scope(const uintptr_t ref_ptr)
	{
		auto lu_result = _cached_scopes.find(ref_ptr);
		return lu_result == _cached_scopes.end() ? nullptr : lu_result->second;
	}

	void IndexCore::cache_scope(const uintptr_t ref_ptr, IndexScopeTreeNode* const scope)
	{
		auto lu_result = _cached_scopes.find(ref_ptr);
		if(lu_result != _cached_scopes.end())
			throw index_exception(fmt::format("Tried to cache a scope multiple times. Tried to cache {}, had {}", scope->get_full_path(), lu_result->second->get_full_path()));
		else
			_cached_scopes.emplace(ref_ptr, scope);
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
				file_content.push_back({{"loc",loc},{"des",ref.key->get_name()},{"to",ref.key->get_source_location()}});
			}
			ret[path.generic_string()] = file_content;
		}

		return ret;
	}

	IndexScopeTreeNode* IndexCore::get_scope_by_position(const IndexLocation& pos)
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

	IndexScopeTreeNode* IndexCore::lookup_scope(const std::string_view& path)
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
