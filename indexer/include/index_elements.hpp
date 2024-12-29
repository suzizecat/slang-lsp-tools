#pragma once

#include <vector>

#include <memory>
#include <filesystem>
#include <optional>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <compare>

#include "slang/syntax/SyntaxNode.h"
#include "slang/ast/Symbol.h"
#include "slang/text/SourceManager.h"

#include "nlohmann/json.hpp"

#define JSON_TO_STRUCT_SAFE_BIND(json_obj, json_key, struct_obj) \
    if(json_obj.contains(json_key))    json_obj.at(json_key).get_to(struct_obj)

NLOHMANN_JSON_NAMESPACE_BEGIN

// Allows serializing and deserializing contents behind a std::optional.
template <typename T>
struct adl_serializer<std::optional<T>> {
	static void to_json(json& j, const std::optional<T>& opt) {
		if (opt) {
			j = *opt; // this will call adl_serializer<T>::to_json which will
					// find the free function to_json in T's namespace!
		} else {
			j = nullptr;
		}
	}

	static void from_json(const json& j, std::optional<T>& opt) {
		if (j.is_null()) {
			opt = std::optional<T>();
		} else {
			opt = j.template get<T>(); // same as above, but with
										// adl_serializer<T>::from_json
		}
	}
};

// Allows serializing contents behind a std::unique_ptr.
// See also: https://github.com/nlohmann/json/issues/975
template <typename T> struct adl_serializer<std::unique_ptr<T>> {
    template <typename BasicJsonType> static void to_json(BasicJsonType& json_value, const std::unique_ptr<T>& ptr)
    {
        if (ptr.get()) {
            json_value = *ptr;
        } else {
            json_value = nullptr;
        }
    }
};

NLOHMANN_JSON_NAMESPACE_END


namespace diplomat
{

/**
 * @brief Combines two hashes into one
 * 
 * @param lhs 'First' hash
 * @param rhs  'Second' hash
 * @return size_t Combined hash
 */
std::size_t hash_combine(size_t lhs, size_t rhs);

namespace index
{

	struct IndexLocation
	{
		std::size_t line;
		std::size_t column;
		std::filesystem::path file;

		IndexLocation(); // Required for easy JSON serialization
		IndexLocation(const std::filesystem::path& file, std::size_t line, std::size_t column);
		explicit IndexLocation(const slang::SourceLocation& loc, const slang::SourceManager& sm);

		bool operator==(const IndexLocation& rhs) const;
		std::strong_ordering operator<=>(const IndexLocation& rhs) const;

		std::string to_string() const;

	};

	struct IndexRange
	{
		IndexLocation start;
		IndexLocation end;

		IndexRange() = default;
		IndexRange(const slang::SourceRange& range, const slang::SourceManager& sm);
		IndexRange(const slang::syntax::SyntaxNode& node, const slang::SourceManager& sm);
		IndexRange(const slang::ast::Symbol& node, const slang::SourceManager& sm);
		IndexRange(const IndexLocation& base, std::size_t nchars, std::size_t nlines = 0);
		

		bool contains(const IndexLocation& loc);
		bool contains(const IndexRange& loc);

		bool operator==(const IndexRange& rhs) const;
	};


	// class IndexSymbol;

	// class IndexReference
	// {
	// 	IndexSymbol* symb;
	// 	IndexRange _source_range;
	// };


	void to_json(nlohmann::json& j, const IndexLocation& s);
	void from_json(const nlohmann::json& j, IndexLocation& s); 
	
	void to_json(nlohmann::json& j, const IndexRange& s);
	void from_json(const nlohmann::json& j, IndexRange& s); 

}
}



// Custom specialization of std::hash can be injected in namespace std.
template<>
struct std::hash<diplomat::index::IndexLocation>
{
	std::size_t operator()(const diplomat::index::IndexLocation& s) const noexcept
	{
	   std::size_t ret = 0;
	   diplomat::hash_combine(ret,std::hash<std::filesystem::path>{}(s.file));
	   diplomat::hash_combine(ret,std::hash<size_t>{}(s.line));
	   diplomat::hash_combine(ret,std::hash<size_t>{}(s.column));
	   return ret;
	}
};


// Custom specialization of std::hash can be injected in namespace std.
template<>
struct std::hash<diplomat::index::IndexRange>
{
	std::size_t operator()(const diplomat::index::IndexRange& s) const noexcept
	{
	   std::size_t ret = 0;
	   diplomat::hash_combine(ret,std::hash<diplomat::index::IndexLocation>{}(s.start));
	   diplomat::hash_combine(ret,std::hash<diplomat::index::IndexLocation>{}(s.end));
	   return ret;
	}
};