#include "index_file.hpp"
#include <spdlog/spdlog.h>
#include <cassert>
namespace diplomat::index {
	IndexFile::IndexFile(const std::filesystem::path& path)
	{
		_filepath = std::filesystem::weakly_canonical(path);
	}

	IndexSymbol *IndexFile::add_symbol(const std::string_view &name, const IndexRange &location, const std::string_view& kind)
	{

		auto [eltpair, inserted] = _declarations.emplace(location,std::make_unique<IndexSymbol>(std::string(name),location));
		if(inserted)
		{
			#ifdef DIPLOMAT_DEBUG
			eltpair->second->set_kind(kind);
			#endif
			add_reference(eltpair->second.get(), eltpair->first);
		}

		return eltpair->second.get();
	}

	void IndexFile::register_scope(IndexScope *_scope)
	{
		_scopes.insert({_scope->get_full_path(),_scope});
	}

	IndexScope* IndexFile::lookup_scope_by_range(const IndexRange& range)
	{
		IndexScope* ret;
		for(auto&  scope : std::views::values(_scopes))
		{
			if((ret = scope->get_scope_for_range(range)) != nullptr)
				return ret;
		}
		return nullptr;
	}

	IndexScope* IndexFile::lookup_scope_by_exact_range(const IndexRange& loc)
	{
		for(auto&  scope : std::views::values(_scopes))
		{
			if(loc == scope->get_source_range())
				return scope;
		}
		return nullptr;
	}

	IndexScope* IndexFile::lookup_scope_by_location(const IndexLocation& loc)
	{
		IndexScope* ret;
		for(auto&  scope : std::views::values(_scopes))
		{
			if((ret = scope->get_scope_for_location(loc)) != nullptr)
				return ret;
		}
		return nullptr;
	}

	IndexSymbol* IndexFile::lookup_symbol_by_location(const IndexLocation& loc)
	{
		// Lookup method from https://stackoverflow.com/a/45426884
		auto lu_result = _references.upper_bound(loc);
		if(lu_result != _references.begin())
			lu_result--;
		else
			return nullptr;

		if(lu_result->second.loc.contains(loc))
		{
			return lu_result->second.key;
		}
		else
			return nullptr;
	}

	void IndexFile::add_reference(IndexSymbol* symb, const IndexRange& range)
	{
		assert(range.start.file == _filepath);
		if(! _references.try_emplace(range.start,range,symb).second)
		{
			spdlog::warn("    Failed to insert a reference to {}", symb->get_name());
			#ifdef DIPLOMAT_DEBUG
			_add_failed_ref(fmt::format("{} at {}",symb->get_name(),range.start.to_string()));
			#endif
		}
		else
			symb->add_reference(range);
	}

	void IndexFile::record_additionnal_lookup_scope(const std::string& path, IndexScope* target)
	{
		spdlog::debug("Recording additionnal lookup scope {}.",path);
		_additional_lookup_scopes[path] = target;
	}

	void IndexFile::invalidate_additionnal_lookup_scope(const std::string& path)
	{
		_additional_lookup_scopes.erase(path);
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

		

		#ifdef DIPLOMAT_DEBUG
		if(s._failed_references.size() > 0)
		{
		j["reffailed"] = s._failed_references;
		
		spdlog::info("Dumped {} failed refs for file {}",s._failed_references.size(),s.get_path().generic_string());
		}
		#endif

	}
}