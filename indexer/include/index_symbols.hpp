#pragma once
#include <string>
#include <optional>
#include <unordered_set>

#include "index_elements.hpp"

#include "nlohmann/json.hpp"

namespace diplomat::index 
{
	class IndexSymbol
	{
		// Allows access to internals for the hash function
		friend struct std::hash<diplomat::index::IndexSymbol>;
		friend void to_json(nlohmann::json& j, const IndexSymbol*& s);
		friend void to_json(nlohmann::json& j, const IndexSymbol& s);
		friend void from_json(const nlohmann::json& j, IndexSymbol& s); 

	protected:
		// The file linking will be done in the index_file, on a second time.
		// The file-linking "key" will be the flesystem::path from the _source_range.
		std::string _name;
		std::optional<IndexRange> _source_range;
		std::unordered_set<IndexRange> _references_locations;

	public : 
		IndexSymbol() = default;
		IndexSymbol(const std::string& name);
		IndexSymbol(const std::string& name, const IndexRange& range);
		IndexSymbol(const std::string_view name, const IndexRange range);
		IndexSymbol(const slang::syntax::SyntaxNode& node, const slang::SourceManager& sm);
		IndexSymbol(const slang::ast::Symbol& node, const slang::SourceManager& sm);
		~IndexSymbol() = default;

		void add_reference(IndexRange ref_location);
		void set_source(const IndexRange& new_source);
		
		inline const std::optional<IndexRange>& get_source() const {return _source_range;};
		inline const std::string& get_name() const {return _name;};
		inline const std::unordered_set<IndexRange>& get_references() const {return _references_locations;};


	};

	void to_json(nlohmann::json& j, const IndexSymbol& s);
	void to_json(nlohmann::json& j, const IndexSymbol*& s);
	void from_json(const nlohmann::json& j, IndexSymbol& s); 

}

// Custom specialization of std::hash can be injected in namespace std.
template<>
struct std::hash<diplomat::index::IndexSymbol>
{
	std::size_t operator()(const diplomat::index::IndexSymbol& s) const noexcept
	{
		std::size_t ret = 0;
		// The positioning only cannot be used as a hash value, as we want to be able
		// to use an "unbound" symbol to ease development.
		// However, as a symbol declaration shall be unique across a scope, there should not be
		// any issue with using the symbol name as a hash key.
		diplomat::hash_combine(ret,std::hash<std::string>{}(s._name));
		if(s._source_range)
			diplomat::hash_combine(ret,std::hash<diplomat::index::IndexRange>{}(s._source_range.value()));
		return ret;
	}
};