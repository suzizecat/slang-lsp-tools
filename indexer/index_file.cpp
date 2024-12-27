#include "index_file.hpp"
#include <spdlog/spdlog.h>
#include <cassert>
namespace diplomat::index {
	IndexFile::IndexFile(const std::filesystem::path& path)
	{
		_filepath = std::filesystem::weakly_canonical(path);
	}

	IndexSymbol *IndexFile::add_symbol(const std::string_view &name, const IndexRange &location)
	{

		if(! _declarations.contains(location))
			_declarations.emplace(location,std::make_unique<IndexSymbol>(std::string(name),location));
		return _declarations.at(location).get();
	}

	void IndexFile::register_scope(IndexScope *_scope)
	{
		_scopes.insert({_scope->get_full_path(),_scope});
	}

	IndexScope* IndexFile::lookup_scope_by_range(const IndexRange& range)
	{
		IndexScope* ret;
		for(auto& [_ , scope] : _scopes)
		{
			if((ret = scope->get_scope_for_range(range)) != nullptr)
				return ret;
		}
		return nullptr;
	}

	void IndexFile::add_reference(IndexSymbol* symb, IndexRange& range)
	{
		assert(range.start.file == _filepath);
		if(! _references.try_emplace(range.start,range,symb).second)
		{
			spdlog::error("Failed to insert a reference to {}", symb->get_name());
		}

	}

	void to_json(nlohmann::json &j, const IndexFile &s)
	{
		j = nlohmann::json {
			{"path", s._filepath}

		};

		j["scopes"] = nlohmann::json::array();
		j["symbols"] = nlohmann::json::array();

		for (const auto& [key, value] : s._scopes)
		{
			j["scopes"].push_back(key);
		}

		for (const auto& [key, value] : s._declarations)
		{
			j["symbols"].push_back(value);
		}

	}
}