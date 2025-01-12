#include "include/index_symbols.hpp"
#include "index_symbols.hpp"
#include <spdlog/spdlog.h>

namespace diplomat::index 
{

	IndexSymbol::IndexSymbol(const std::string& name) : 
		_name(name), _references_locations({}) {}

	IndexSymbol::IndexSymbol(const std::string& name, const IndexRange& range) : 
		_name(name), _source_range(range), _references_locations({}) {}

	IndexSymbol::IndexSymbol(const std::string_view name, const IndexRange range) : 
		_name(std::string(name)), _source_range(range) {}

	IndexSymbol::IndexSymbol(const slang::syntax::SyntaxNode& node, const slang::SourceManager& sm) :
		_name(node.getFirstToken().rawText()),
		_source_range({node,sm}),
		_references_locations({})
	{}


	IndexSymbol::IndexSymbol(const slang::ast::Symbol& node, const slang::SourceManager& sm) :
		_name(node.name),
		_source_range({node,sm}),
		_references_locations({})
	{}


	void IndexSymbol::add_reference(IndexRange ref_location)
	{
		spdlog::trace("        Add reference to {} : {}",_name,ref_location.start.to_string());
		_references_locations.insert(ref_location);
	}

	void IndexSymbol::set_source(const IndexRange &new_source)
	{
		_source_range = new_source;
	}
}


void diplomat::index::to_json(nlohmann::json &j, const IndexSymbol &s)
{
	j = nlohmann::json{
		#ifdef DIPLOMAT_DEBUG
		{"kind", s._kind},
		#endif
		{"id",s._name},
		{"loc",s._source_range},
		{"refs",s._references_locations}
	};
}

void diplomat::index::to_json(nlohmann::json &j, const IndexSymbol *&s)
{
		j = nlohmann::json{
		#ifdef DIPLOMAT_DEBUG
		{"kind", s->_kind},
		#endif
		{"id",s->_name},
		{"loc",s->_source_range},
		{"refs",s->_references_locations}
	};
}

void diplomat::index::from_json(const nlohmann::json &j, IndexSymbol &s)
{
	JSON_TO_STRUCT_SAFE_BIND(j,"id",s._name);
	JSON_TO_STRUCT_SAFE_BIND(j,"loc",s._source_range);
	JSON_TO_STRUCT_SAFE_BIND(j,"refs",s._references_locations);
}
