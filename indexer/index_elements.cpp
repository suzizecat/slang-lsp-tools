#include "index_elements.hpp"

#include <fmt/format.h>


namespace diplomat {
	size_t hash_combine(size_t lhs, size_t rhs)
	{
		if constexpr (sizeof(size_t) >= 8)
		{
			lhs ^= rhs + 0x517cc1b727220a95 + (lhs << 6) + (lhs >> 2);
		}
		else
		{
			lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
		}
		return lhs;
	}
}
namespace diplomat::index
{
	IndexLocation::IndexLocation() : line(0), column(0), file()
	{}

	IndexLocation::IndexLocation(const std::filesystem::path& file, std::size_t line,
	                             std::size_t column) : 
	line(line), 
	column(column), 
	file(std::filesystem::weakly_canonical(file))
	{
	}

IndexLocation::IndexLocation(const slang::SourceLocation& loc, const slang::SourceManager& sm)
{
	slang::SourceLocation location = sm.getFullyOriginalLoc(loc);
	file = std::filesystem::weakly_canonical(sm.getFullPath(location.buffer()));
	line = sm.getLineNumber(location);
	column = sm.getColumnNumber(location);
}

bool IndexLocation::operator==(const IndexLocation &rhs) const
{
	return rhs.line == line && rhs.column == column && rhs.file == file;
}

std::string index::IndexLocation::to_string() const
{
	return fmt::format("{}:{}:{}",file.generic_string(),line,column);
;
}

std::strong_ordering IndexLocation::operator<=>(const IndexLocation& rhs) const
{
	if(rhs.file != file)
		throw std::logic_error("Trying to compare position from different files");
	if(line < rhs.line)
		return std::strong_ordering::less;
	if(line > rhs.line )
		return std::strong_ordering::greater;
	if(column < rhs.column)
		return std::strong_ordering::less;
	if(column > rhs.column)
		return std::strong_ordering::greater;
	return std::strong_ordering::equivalent;	
}

IndexRange::IndexRange(const slang::SourceRange& range, const slang::SourceManager& sm) :
	start(range.start(),sm),
	end(range.end(),sm)
{}

IndexRange::IndexRange(const slang::syntax::SyntaxNode& node, const slang::SourceManager& sm) :
	IndexRange(node.sourceRange(),sm)
{}

IndexRange::IndexRange(const slang::ast::Symbol& node, const slang::SourceManager& sm) :
	IndexRange(*(node.getSyntax()),sm)
{}

index::IndexRange::IndexRange(const IndexLocation& base, std::size_t nchars, std::size_t nlines)
{
	start = base;
	end = base;
	end.column += nchars;
	end.line += nlines;
}

bool IndexRange::contains(const IndexLocation& loc)
{
	if(loc.file != start.file)
		return false;
	
	if(loc.line < start.line || loc.line > end.line)
		return false;

	if(loc.line == start.line && loc.column < start.column)
		return false;

	if(loc.line == end.line && loc.column > end.column)
		return false;

	return true;

}

bool IndexRange::contains(const IndexRange &loc)
{
	if(loc.start.file != start.file)
		return false;
	
	if(loc.start.line < start.line || loc.end.line > end.line)
		return false;

	if(loc.start.line == start.line && loc.start.column < start.column)
		return false;

	if(loc.end.line == end.line && loc.end.column > end.column)
		return false;

	return true;
}

bool IndexRange::operator==(const IndexRange &rhs) const
{
	return rhs.start == start && rhs.end == end;
}

void to_json(nlohmann::json &j, const IndexLocation &s)
{
	j = s.to_string();
	// j = nlohmann::json{
	// 	{"f",s.file.generic_string()},
	// 	{"l",s.line},
	// 	{"c",s.column}
	// };
}

void from_json(const nlohmann::json &j, IndexLocation &s)
{
	s.file = std::filesystem::path(j.at("f").template get<std::string>());
	j.at("l").get_to(s.line);
	j.at("c").get_to(s.column);
}

void to_json(nlohmann::json &j, const IndexRange &s)
{
	j = nlohmann::json{
		{"beg",s.start},
		{"end",s.end}
	};
}

void from_json(const nlohmann::json &j, IndexRange &s)
{
	j.at("beg").get_to(s.start);
	j.at("end").get_to(s.end);
}


}

