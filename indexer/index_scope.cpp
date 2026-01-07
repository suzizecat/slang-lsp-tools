#include "index_scope.hpp"
#include <cstddef>
#include <ranges>
#include "fmt/format.h"
#include "index_symbols.hpp"

#include <spdlog/spdlog.h>

// // Custom specialization of std::hash can be injected in namespace std.
// template<>
// struct std::hash<diplomat::index::IndexScope>
// {
//     std::size_t operator()(const diplomat::index::IndexScope& s) const noexcept
//     {
//        return s.get_hash_value();
//     }
// };

namespace diplomat::index {



	IndexSymbol* IndexScope::add_symbol(IndexSymbol* symb)
	{
		auto lu_result = _content.find(symb->get_name());
		if(lu_result != _content.end())
			return lu_result->second.get();
		else
		{
			auto result = _content.emplace(symb->get_name(), std::unique_ptr<IndexSymbol>(symb));
			return result.first->second.get();
		}
	}

	IndexSymbol* IndexScope::add_symbol(std::unique_ptr<IndexSymbol> symb)
	{
		return add_symbol(symb.release());
	}

	IndexSymbol* IndexScope::get_symbol(const std::string &name)
	{
		auto lu_result = _content.find(name);
		if(lu_result != _content.end())
			return lu_result->second.get();
		return nullptr;
		
	}



	void to_json(nlohmann::json &j, const IndexScope &s)
	{

	j = nlohmann::json(); 
	// {
	// 	#ifdef DIPLOMAT_DEBUG
	// 	{"_kind",s._kind},
	// 	#endif
	// 	{"name",s._name},
	// 	{"def",s._source_range},
	// 	{"virtual",s._is_virtual},
	// 	{"children",s._children}
	// };

	nlohmann::json aliases;

	// for(auto& [key, value] : s._child_aliases)
	// {
	// 	aliases[key] = value->get_name();
	// }

	// j["children_aliases"] = aliases;

	nlohmann::json content;

	for(auto& [key, value] : s._content)
	{
		content[key] = *value;
	}
	j["content"] = content ;
}

// void from_json(const nlohmann::json &j, IndexScope &s)
// {
// 		JSON_TO_STRUCT_SAFE_BIND(j,"name",s._name);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"def",s._source_range);
// 		// JSON_TO_STRUCT_SAFE_BIND(j,"content",s._content);
// 		// JSON_TO_STRUCT_SAFE_BIND(j,"sub",s._children);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"virtual",s._is_virtual);
// 		JSON_TO_STRUCT_SAFE_BIND(j,"parentAccess",s._can_access_parent);

// 		for (auto& [key, value] : j["content"].items())
// 		{
// 			IndexSymbol* newSymb = s.add_symbol(key);
// 			*newSymb = value.template get<IndexSymbol>();
// 		}

// 		// Might require some more subtle approach...
// 		for (auto& [key, value] : j["sub"].items())
// 		{
// 			IndexScope* new_scope = s.add_child(key);
// 			*new_scope = value.template get<IndexScope>();
// 		}
// }
};
