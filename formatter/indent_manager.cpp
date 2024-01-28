#include "indent_manager.hpp"

using namespace slang::syntax;
using namespace slang::parsing;

IndentManager::IndentManager(slang::BumpAllocator& alloc, const unsigned int space_per_level, const bool use_tabs ) :
    _level(0),
    _space_per_level(space_per_level),
    _use_tabs(use_tabs),
    _mem(alloc)
{}

slang::parsing::Token IndentManager::replace_spacing(const slang::parsing::Token &tok, unsigned int spaces)
{
	// Store the string in "persistent" memory (BumpAllocator)
	if(spaces == 0)
		return tok.withTrivia(_mem,{});
	
	slang::byte* spacing_base = _mem.allocate(spaces, alignof(char));
	std::memset(spacing_base,' ',spaces);

	// Declare and store the trivia, then create the token to add.
	Trivia* t = _mem.emplace<Trivia>(TriviaKind::Whitespace,std::string_view((char*)spacing_base,spaces));
	return tok.withTrivia(_mem,{t,1});
}


slang::parsing::Token IndentManager::indent(const slang::parsing::Token& tok)
{

	// Store the string in "persistent" memory (BumpAllocator)
	unsigned int mem_space = _level * (_use_tabs ? 1 : _space_per_level);
	slang::byte* spacing_base = _mem.allocate(mem_space, alignof(char));
	
	std::memset(spacing_base,_use_tabs ? '\t' : ' ',mem_space);
	
	Trivia indent_trivia(TriviaKind::Whitespace,std::string_view((char*)spacing_base,mem_space));
	
	// The goal is to skip all whitespaces at line start to replace them by the indent
	slang::SmallVector<Trivia> kept_trivia;
	bool skip_spacings = false;

	for(const auto& trivia : tok.trivia())
	{
		if(trivia.kind == TriviaKind::Whitespace)
		{
			if(!skip_spacings)
				kept_trivia.push_back(trivia);
		}
		else
		{
			if(trivia.kind == TriviaKind::EndOfLine)
				skip_spacings = true;
			else
				skip_spacings = false;

			kept_trivia.push_back(trivia);

			if(trivia.kind == TriviaKind::EndOfLine)
				kept_trivia.push_back(indent_trivia);
		}
	}

	// Declare and store the indentation trivia, then create the token to add.
	return tok.withTrivia(_mem,kept_trivia.copy(_mem));
}