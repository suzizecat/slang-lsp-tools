#include "format_DataDeclaration.hpp"

#include <slang/parsing/TokenKind.h>
#include <slang/util/SmallVector.h>
#include <fmt/format.h>
#include <ranges>
#include <numeric>
#include <span>
#include <iostream>
#include <cstring>
using namespace slang::syntax;
using Trivia = slang::parsing::Trivia;
using Token = slang::parsing::Token;
using TriviaKind = slang::parsing::TriviaKind;

DataDeclarationSyntaxVisitor::DataDeclarationSyntaxVisitor() : slang::syntax::SyntaxRewriter<DataDeclarationSyntaxVisitor>(),
	_type_sizes{},
	_array_sizes{},
	_type_name_size(0),
	_var_name_size(0),
	_bloc_type_kind(SyntaxKind::Unknown),
	_to_format{},
	_mem(),
	formatted{}
	{}

    void DataDeclarationSyntaxVisitor::clear()
    {
		_to_format.clear();
		_type_sizes.clear();
		_array_sizes.clear();
		_type_name_size = 0;
		_var_name_size = 0;
    }

void DataDeclarationSyntaxVisitor::handle(const slang::syntax::DataDeclarationSyntax &node)
{
	int nb_newlines = 0;

	// Process pending format if we come across at least 2 newlines.
	for(const auto trivia : node.getFirstToken().trivia())
	{
		if(trivia.kind == slang::parsing::TriviaKind::EndOfLine)
		{
			nb_newlines++;
		}
		if (nb_newlines == 2)
		{
			process_pending_formats();
			break;
		}
	}

	_store_format(node);
	 
}

void DataDeclarationSyntaxVisitor::process_pending_formats()
{
	_debug_print();
	for(const auto* decl : _to_format)
	{
		formatted += _format(decl);
	}
	clear();
}

std::string DataDeclarationSyntaxVisitor::_format(const slang::syntax::DataDeclarationSyntax* decl)
{
	slang::syntax::DataDeclarationSyntax* work = deepClone(*decl,_mem);
	// Use Token::withTrivia to set trivia
	std::string ret = "";
	std::string temp = "";
	//TODO Check last trivia of first token for indent hint.
	//Set the trivia of the first token after type to appropriate spacing (1 + type length + full type_size if not an open bracket)
	unsigned int spacing = 1 + _type_name_size;
	for(const auto& trivia : decl->getFirstToken().trivia())
	{
		temp += std::string(trivia.getRawText());
	}
	
	// Create vector with appropriate tokens.
	// Copy the vector content to _mem. 
	// Use the resulting pointer for the span. 

	int next_alignement_size = _type_name_size;
	//Temp token list for modifiers;
	slang::SmallVector<slang::parsing::Token> tok_list;
std::cout << "Modifier pre-update: " << work->modifiers.toString() << std::endl;
	bool first_element = true;
	for(const auto& tok : work->modifiers)
	{
		if(first_element)
		{
			first_element = false;
			tok_list.push_back(indent(_mem, tok, '\t', 1, 1));
			next_alignement_size -= tok.rawText().length();
		}
		else
		{
			tok_list.push_back(replace_spacing(_mem, tok, 1));
			next_alignement_size -= 1+ tok.rawText().length();
		}
	}

	// Update the modifiers token list, see https://github.com/MikePopoloski/slang/discussions/828#discussioncomment-8233513
	work->modifiers = tok_list.copy(_mem);

	std::cout << "Modifier post-update: " << work->modifiers.toString() << std::endl;
	switch (decl->type->kind)
	{
	case SyntaxKind::LogicType :
		{
			// Compute the size of keyword + signing + appropriate spaces and align right next token.
		IntegerTypeSyntax& type = work->type->as<IntegerTypeSyntax>();
		if (first_element)
		{
			type.keyword = indent(_mem, type.keyword, '\t', 1, 1);

			first_element = false;
		}
		else
		{
			type.keyword = replace_spacing(_mem, type.keyword, 1);
		}

		next_alignement_size -= type.keyword.rawText().length();

		if (type.signing)
		{
			type.signing = replace_spacing(_mem, type.signing, 1);
			next_alignement_size -= 1 + type.signing.rawText().length();
		}

		first_element = true; // Detect the first element to right-align

		auto& dimensions = work->type->as<IntegerTypeSyntax>().dimensions;
		unsigned int size_dim_index = 0;
		if (dimensions.size() > 0)
		{
			for (auto* dim : dimensions)
			{
				if (first_element)
					dim->openBracket = token_align_right(_mem, dim->openBracket, next_alignement_size + 1, false);
				else
					dim->openBracket = replace_spacing(_mem, dim->openBracket, 0);

				switch (dim->specifier->kind)
				{
				case SyntaxKind::RangeDimensionSpecifier :
				{
					RangeDimensionSpecifierSyntax& spec = dim->specifier->as<RangeDimensionSpecifierSyntax>();
					if (spec.selector->kind == SyntaxKind::SimpleRangeSelect)
					{
						RangeSelectSyntax& select = spec.selector->as<RangeSelectSyntax>();
						unsigned int temp_size = select.left->toString().length();
						select.range = token_align_right(_mem, select.range, _type_sizes.at(size_dim_index++) - temp_size);

						temp_size = select.right->toString().length();
						dim->closeBracket = token_align_right(_mem, dim->closeBracket, _type_sizes.at(size_dim_index++) - temp_size);
						// TODO - Finish the job here
					}
				}
					break;
				
				default:
					break;
				}

				first_element = false;
			}

		}

		size_t max_len = (1 + (1 + 3 * _type_sizes.size()) / 2) + std::reduce(_type_sizes.cbegin(), _type_sizes.cend());

		ret += fmt::format("{:{}s} ", raw_text_from_syntax(dimensions), max_len);
		
	}
		
		break;
	
	default:
		//current_type_size += node.type->toString().length();
		// In case of doubt, do nothing !
		break;
	}

	ret += fmt::format("{:{}s} ",decl->declarators[0]->name.rawText(),_var_name_size);
	ret += raw_text_from_syntax(decl->declarators[0]->dimensions);
	ret += ";";
	ret += "\n";
	std::cout << work->toString() << std::endl;

		return ret;
}


void DataDeclarationSyntaxVisitor::_read_type_array(const slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax> dimensions)
{
	unsigned int index = 0;
	for(const VariableDimensionSyntax* dim : dimensions | std::views::reverse )
	{
		switch (dim->specifier->kind)
		{
		case SyntaxKind::RangeDimensionSpecifier:
			{
				const auto* selector = dim->specifier->as<RangeDimensionSpecifierSyntax>().selector.get();
			}

			break;
		
		default:
			break;
		}
	}
}

void DataDeclarationSyntaxVisitor::_debug_print()
{
	std::cout << "Type sizes total = " << int(std::reduce(_type_sizes.cbegin(),_type_sizes.cend())) << std::endl;
	std::cout << "Arra sizes total = " << int(std::reduce(_array_sizes.cbegin(),_array_sizes.cend())) << std::endl;
	std::cout << "Type   name size = " << _type_name_size << std::endl;
	std::cout << "Var    name size = " << _var_name_size << std::endl;
	std::cout << std::endl;
}

/**
 * @brief Analyze the node and compute the internal values
 * 
 * @param node Node to analyze
 */
void DataDeclarationSyntaxVisitor::_store_format(const slang::syntax::DataDeclarationSyntax &node)
{
	if(_bloc_type_kind == SyntaxKind::Unknown)
		_bloc_type_kind = node.type->kind;

	if(! node.type->isKind(_bloc_type_kind))
		process_pending_formats();


	size_t current_type_size = 0;
	for(const auto& token : node.modifiers )
	{
		// Add the length of the modifiers + one space each.
		current_type_size += token.rawText().length() +1 ; 
	}

	
	switch (node.type->kind)
	{
	case SyntaxKind::LogicType :
		{
		current_type_size += type_length(&(node.type->as<IntegerTypeSyntax>()));
		
		const auto& dimensions = node.type->as<IntegerTypeSyntax>().dimensions;
		
		dimension_syntax_to_vector(dimensions, _type_sizes);
		}
		break;
	
	default:
		//current_type_size += node.type->toString().length();
		// In case of doubt, do nothing !
		break;
	}


	_type_name_size = std::max(_type_name_size,current_type_size);
	
	for(const auto& decl : node.declarators)
	{
		_var_name_size = std::max(decl->name.rawText().length(),_var_name_size);
		dimension_syntax_to_vector(decl->dimensions,_array_sizes);
	}

	_to_format.push_back(&node);

}

unsigned int type_length(const slang::syntax::IntegerTypeSyntax* node)
{       
	unsigned int result = 0;
	// As we don't expect missing keyword, no need to add a space.
	if(! node->keyword.isMissing())
		result += node->keyword.range().end() - node->keyword.range().start();
	
	// Expecting signing with space before.
	if(! node->signing.isMissing())
		result += node->signing.range().end() - node->signing.range().start() + 1; 

	return result;            
}

/**
 * @brief Convert a syntax-list of variable dimensions to content size in a vector
 * 
 * @param dimensions Dimension to process
 * @param target_vector vector to handle and fill
 */
void dimension_syntax_to_vector(const slang::syntax::SyntaxList<slang::syntax::VariableDimensionSyntax> dimensions, std::vector<size_t>& target_vector)
{
	unsigned int index = 0;

	// Use 2* dimension for worst case (normal range)
	if (2*dimensions.size() > target_vector.size())
		target_vector.resize(2*dimensions.size(),0);

	for(const auto* dim : dimensions )
	{
		const auto specifier = dim->specifier;
		if(specifier == nullptr)
			continue;

		switch (specifier->kind)
		{
		case SyntaxKind::RangeDimensionSpecifier:
			switch (specifier->as<RangeDimensionSpecifierSyntax>().selector->kind)
			{
			case SyntaxKind::BitSelect :
				{
					// Only register one dimension because it would complexify the code
					// and no one use this mixed with multi-specifier anyway :< 
					const auto* expr = specifier->as<RangeDimensionSpecifierSyntax>().selector->as<BitSelectSyntax>().expr.get();
					target_vector[index] = std::max(target_vector[index], raw_text_from_syntax(*expr).length());
					index++;
				}
				break;
			case SyntaxKind::AscendingRangeSelect : [[fallthrough]]; 
			case SyntaxKind::DescendingRangeSelect: [[fallthrough]];
			case SyntaxKind::SimpleRangeSelect :
				{
					const auto& sel = specifier->as<RangeDimensionSpecifierSyntax>().selector->as<RangeSelectSyntax>();
					target_vector[index] = std::max(target_vector[index], raw_text_from_syntax(*(sel.right.get())).length());
					index++;
					target_vector[index] = std::max(target_vector[index], raw_text_from_syntax(*(sel.left.get())).length());
					index++;
				}
				break;
			default:
				throw std::domain_error("Unexpected selector kind value");
				break;
			}
			break;
		
		default:
			break;
		}
		// Remove the two brackets from the size.
		
	}
}

/**
 * @brief Get only text from a syntax node (without trivia)
 * 
 * @param node Node to print
 */
std::string raw_text_from_syntax(const slang::syntax::SyntaxNode &node)
{
	std::string result;
	for(int i = 0; i < node.getChildCount(); i++)
	{
		const SyntaxNode* sub_node = node.childNode(i);
		
		if(sub_node != nullptr)
		{
			result += raw_text_from_syntax(*sub_node);
		}
		else
		{
			for(const auto& trivia : node.childToken(i).trivia())
			{
				if(trivia.kind == slang::parsing::TriviaKind::Whitespace)
				{
					result += " ";
					break;
				}
			}
			result += node.childToken(i).rawText();
		}
	}
    return result;
}

/**
 * @brief This function allows aligning a token to the right by padding left with spaces.
 * 
 * @param alloc Bump allocator reference
 * @param tok Token to allign
 * @param align_size Total final size of the spacing + token
 * @param allow_no_space If set to false, at least one space will be inserted before the token
 * regardless of overflowing the requested size.
 * @return slang::parsing::Token the token with appropriate alignment as trivia.
 */
Token token_align_right( slang::BumpAllocator& alloc ,const Token& tok, const unsigned int align_size, const bool allow_no_space )
{
	const unsigned int tok_len = tok.rawText().length();
	const unsigned int target_spacing = std::max(allow_no_space ? 0U : 1U, align_size - tok_len);
	return replace_spacing(alloc,tok,target_spacing);
}

Token replace_spacing(slang::BumpAllocator& alloc , const Token& tok, const unsigned int space_number)
{
	// Store the string in "persistent" memory (BumpAllocator)
	if(space_number == 0)
		return tok.withTrivia(alloc,{});
	
	slang::byte* spacing_base = alloc.allocate(space_number, alignof(char));
	std::memset(spacing_base,' ',space_number);

	// Declare and store the trivia, then create the token to add.
	Trivia* t = alloc.emplace<Trivia>(TriviaKind::Whitespace,std::string_view((char*)spacing_base,space_number));
	t->getRawText();
	return tok.withTrivia(alloc,{t,1});
}


slang::parsing::Token indent( slang::BumpAllocator& alloc ,const slang::parsing::Token& tok, const char indent_char, const unsigned int char_num, const unsigned int indent_level)
{
	// Store the string in "persistent" memory (BumpAllocator)
	unsigned int mem_space = indent_level * char_num;
	slang::byte* spacing_base = alloc.allocate(mem_space, alignof(char));
	
	std::memset(spacing_base,' ',mem_space);
	
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
	return tok.withTrivia(alloc,kept_trivia.copy(alloc));
}
