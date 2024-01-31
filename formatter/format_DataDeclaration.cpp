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

DataDeclarationSyntaxVisitor::DataDeclarationSyntaxVisitor(IndentManager* indent) : SyntaxRewriter<DataDeclarationSyntaxVisitor>(),
	_modifier_sizes{},
	_type_sizes{},
	_array_sizes{},
	_type_name_size(0),
	_var_name_size(0),
	_bloc_type_kind(SyntaxKind::Unknown),
	_to_format{},
	_mem(),
	_idt(indent)
	{}

    void DataDeclarationSyntaxVisitor::clear()
    {
		_to_format.clear();
		_modifier_sizes.clear();
		_type_sizes.clear();
		_array_sizes.clear();
		_type_name_size = 0;
		_var_name_size = 0;
    }

/**
 * @brief Top level handle, should enter here and exit here.
 * 
 * @param node 
 */
void DataDeclarationSyntaxVisitor::handle(const CompilationUnitSyntax& node)
{
	visitDefault(node);

	// Process any remaining formatting.
	process_pending_formats();
}

void DataDeclarationSyntaxVisitor::handle(const DataDeclarationSyntax &node)
{
	_split_bloc(node);
	_store_format(node);
}

void DataDeclarationSyntaxVisitor::handle(const ImplicitAnsiPortSyntax &node)
{
	// Do not split in case of multiple empty lines.
	//_split_bloc(node);
	_store_format(node);
}


void DataDeclarationSyntaxVisitor::_split_bloc(const SyntaxNode& node)
{
	int nb_newlines = 0;

	// Process pending format if we come across at least 2 newlines.
	for(const auto trivia : node.getFirstToken().trivia())
	{
		if(trivia.kind == TriviaKind::EndOfLine)
		{
			nb_newlines++;
		}
		if (nb_newlines == 2)
		{
			process_pending_formats();
			break;
		}
	}
}

void DataDeclarationSyntaxVisitor::process_pending_formats()
{
	for(const auto* decl : _to_format)
	{
		SyntaxNode* formatted_decl = _format_any(decl);
		
		if(formatted_decl != nullptr)
			replace(*decl,*formatted_decl);
	}
	clear();
}

slang::syntax::SyntaxNode* DataDeclarationSyntaxVisitor::_format_any(const slang::syntax::SyntaxNode* node)
{
	switch (node->kind)
	{
	case SyntaxKind::DataDeclaration:
		return _format(node->as<DataDeclarationSyntax>());
		break;
	case SyntaxKind::ImplicitAnsiPort:
		return _format(node->as<ImplicitAnsiPortSyntax>());
		break;
	
	default:
		return nullptr;
		break;
	}
}

DataDeclarationSyntax* DataDeclarationSyntaxVisitor::_format(const DataDeclarationSyntax& decl)
{
	DataDeclarationSyntax* work = deepClone(decl,_mem);

	unsigned int modifier_id = 0;
	int next_alignement_size = 0;

	// Modifiers
	// ------------------------------------------------

	//Temp token list for modifiers;
	slang::SmallVector<slang::parsing::Token> tok_list;
	bool first_element = true;
	
	// Process the modifiers
	for(const auto& tok : work->modifiers)
	{
		
		if(first_element)
		{
			first_element = false;
			tok_list.push_back(_idt->indent(tok));
		}
		else
		{
			tok_list.push_back(_idt->replace_spacing(tok, next_alignement_size));
		}

		// +1 For spacing
		next_alignement_size = _modifier_sizes[modifier_id] + 1 - tok.rawText().length();
		modifier_id ++;
	}

	if(modifier_id < _modifier_sizes.size())
		next_alignement_size += sum_dim_vector(_modifier_sizes,modifier_id) 
								+ _modifier_sizes.size() - (modifier_id);

	// Force at least a space if some modifiers are presents
	if (modifier_id != 0 && next_alignement_size == 0)
		next_alignement_size = 1;	
	
	// Update the modifiers token list, see https://github.com/MikePopoloski/slang/discussions/828#discussioncomment-8233513
	work->modifiers = tok_list.copy(_mem);

	// Type
	// ------------------------------------------------
	size_t remaining_len = _format_data_type_syntax(work->type,first_element,next_alignement_size);
	

	// Values
	// ------------------------------------------------
	// This spacing is only for the first one. 
	work->declarators[0]->name = _idt->replace_spacing(work->declarators[0]->name,remaining_len);
	remaining_len = 0;

	auto* declarator = work->declarators[0];
	auto& unpacked_dimension = declarator->dimensions;
	remaining_len = align_dimension(_mem,unpacked_dimension,_array_sizes,_var_name_size - declarator->name.rawText().length() +1 );

	work->semi = _idt->replace_spacing(work->semi,remaining_len );

	return work;
}


/**
 * @brief Process for the data type node formatting
 * 
 * @param stx 
 * @param first_element 
 * @param first_alignement_size 
 */
size_t DataDeclarationSyntaxVisitor::_format_data_type_syntax(slang::syntax::DataTypeSyntax* stx,  bool first_element, size_t first_alignement_size)
{
	size_t next_alignement_size = first_alignement_size;
	size_t remaining_len = 0;
	
	switch (stx->kind)
	{
	case SyntaxKind::LogicType :
		{
			// Compute the size of keyword + signing + appropriate spaces and align right next token.
		IntegerTypeSyntax& type = stx->as<IntegerTypeSyntax>();
		if (first_element)
		{
			// Indent will misalign if others have modifiers.
			type.keyword = _idt->indent(type.keyword,next_alignement_size);
			first_element = false;
		}
		else
		{
			type.keyword = _idt->replace_spacing(type.keyword, next_alignement_size);
		}

		next_alignement_size = _type_name_size - type.keyword.rawText().length();

		if (type.signing)
		{
			type.signing = _idt->replace_spacing(type.signing, 1);
			next_alignement_size -= 1 + type.signing.rawText().length();
		}

		first_element = true;
	
		auto& packed_dimensions = type.dimensions;
		remaining_len = align_dimension(_mem,packed_dimensions,_type_sizes,next_alignement_size);

		if (remaining_len == 0)
			remaining_len = 1;
		if(_type_sizes.size() != 0 && packed_dimensions.size() == 0 )
			remaining_len += 1;
	}
		break;
	
	default:
		//current_type_size += node.type->toString().length();
		// In case of doubt, do nothing !
		break;
	}

	return remaining_len;
}


ImplicitAnsiPortSyntax* DataDeclarationSyntaxVisitor::_format(const ImplicitAnsiPortSyntax& decl)
{
	ImplicitAnsiPortSyntax* work = deepClone(decl,_mem);

	unsigned int modifier_id = 0;
	int next_alignement_size = 0;
	bool first_element = true;

	// Modifiers
	// ------------------------------------------------

	PortHeaderSyntax* base_header = work->header;

	auto modifiers_handler = [&](Token& tok){
			if(tok.valid())
			{
				if(first_element) 
				{
					first_element = false;
					tok = _idt->indent(tok);
				}
				else
				{
					tok = _idt->replace_spacing(tok,next_alignement_size);
				}
				
				next_alignement_size = 1 + _modifier_sizes[modifier_id] - tok.rawText().length();
				modifier_id ++;
			}
		};

	switch (base_header->kind)
	{
	case SyntaxKind::VariablePortHeader:
		{
			VariablePortHeaderSyntax& header = base_header->as<VariablePortHeaderSyntax>();

			modifiers_handler(header.constKeyword);
			modifiers_handler(header.direction);
			modifiers_handler(header.varKeyword);

			next_alignement_size = _format_data_type_syntax(header.dataType,first_element,next_alignement_size);
		}
		break;
	
	default:
		break;
	}
	/*
	//Temp token list for modifiers;
	slang::SmallVector<slang::parsing::Token> tok_list;
	bool first_element = true;
	
	// Process the modifiers
	for(const auto& tok : work->modifiers)
	{
		
		if(first_element)
		{
			first_element = false;
			tok_list.push_back(_idt->indent(tok));
		}
		else
		{
			tok_list.push_back(_idt->replace_spacing(tok, next_alignement_size));
		}

		// +1 For spacing
		next_alignement_size = _modifier_sizes[modifier_id] + 1 - tok.rawText().length();
		modifier_id ++;
	}

	if(modifier_id < _modifier_sizes.size())
		next_alignement_size += sum_dim_vector(_modifier_sizes,modifier_id) 
								+ _modifier_sizes.size() - (modifier_id);

	// Force at least a space if some modifiers are presents
	if (modifier_id != 0 && next_alignement_size == 0)
		next_alignement_size = 1;	
	
	// Update the modifiers token list, see https://github.com/MikePopoloski/slang/discussions/828#discussioncomment-8233513
	work->modifiers = tok_list.copy(_mem);

	// Type
	// ------------------------------------------------

	size_t remaining_len = 0;
	
	switch (decl->type->kind)
	{
	case SyntaxKind::LogicType :
		{
			// Compute the size of keyword + signing + appropriate spaces and align right next token.
		IntegerTypeSyntax& type = work->type->as<IntegerTypeSyntax>();
		if (first_element)
		{
			// Indent will misalign if others have modifiers.
			type.keyword = _idt->indent(type.keyword,next_alignement_size);
			first_element = false;
		}
		else
		{
			type.keyword = _idt->replace_spacing(type.keyword, next_alignement_size);
		}

		next_alignement_size = _type_name_size - type.keyword.rawText().length();

		if (type.signing)
		{
			type.signing = _idt->replace_spacing(type.signing, 1);
			next_alignement_size -= 1 + type.signing.rawText().length();
		}

		first_element = true;
	
		auto& packed_dimensions = work->type->as<IntegerTypeSyntax>().dimensions;
		remaining_len = align_dimension(_mem,packed_dimensions,_type_sizes,next_alignement_size);

		if (remaining_len == 0)
			remaining_len = 1;
		if(_type_sizes.size() != 0 && packed_dimensions.size() == 0 )
			remaining_len += 1;
	}
		break;
	
	default:
		//current_type_size += node.type->toString().length();
		// In case of doubt, do nothing !
		break;
	}


	// Values
	// ------------------------------------------------
	// This spacing is only for the first one. 
	work->declarators[0]->name = _idt->replace_spacing(work->declarators[0]->name,remaining_len);
	remaining_len = 0;

	auto* declarator = work->declarators[0];
	auto& unpacked_dimension = declarator->dimensions;
	remaining_len = align_dimension(_mem,unpacked_dimension,_array_sizes,_var_name_size - declarator->name.rawText().length() +1 );

	work->semi = _idt->replace_spacing(work->semi,remaining_len );

	*/
	return work;
}



/**
 * @brief Analyze the node and compute the internal values
 * 
 * @param node Node to analyze
 */
void DataDeclarationSyntaxVisitor::_store_format(const DataDeclarationSyntax &node)
{
	_switch_bloc_type(node.type);
	_read_type(node);
	
	for(const auto& decl : node.declarators)
	{
		_var_name_size = std::max(decl->name.rawText().length(),_var_name_size);
		dimension_syntax_to_vector(decl->dimensions,_array_sizes);
	}

	_to_format.push_back(&node);

}

/**
 * @brief Analyze the node and compute the internal values
 * 
 * @param node Node to analyze
 */
void DataDeclarationSyntaxVisitor::_store_format(const ImplicitAnsiPortSyntax &node)
{
	// Avoid splitting amongst different ports
	_switch_bloc_type(node);
	_read_type(node);
	
	_var_name_size = std::max(node.declarator->name.rawText().length(),_var_name_size);
	dimension_syntax_to_vector(node.declarator->dimensions,_array_sizes);

	
	_to_format.push_back(&node);
}


void DataDeclarationSyntaxVisitor::_read_type(const DataDeclarationSyntax& node)
{
	tokens_to_vector(node.modifiers,_modifier_sizes);
    _read_type_len(node.type);
}


void DataDeclarationSyntaxVisitor::_read_type(const ImplicitAnsiPortSyntax& node)
{
	
	switch(node.header->kind)
	{
		case SyntaxKind::VariablePortHeader :
			{
				const VariablePortHeaderSyntax& header = node.header->as<VariablePortHeaderSyntax>();
				const Token modifiers[] = {header.direction,header.direction,header.varKeyword};
				tokens_to_vector(modifiers,_modifier_sizes);
				_read_type_len(header.dataType);
			}
		break;
		case SyntaxKind::NetPortHeader :
			{
				const NetPortHeaderSyntax& header = node.header->as<NetPortHeaderSyntax>();
				const Token modifiers[] = {header.direction,header.netType};
				tokens_to_vector(modifiers,_modifier_sizes);
				_read_type_len(header.dataType);
			}
		break;
		default: break;
	}

}

/**
 * @brief Processes information related to the type declaration.
 * In particular, extract the type length and the type-side dimensions
 * 
 * @param type Type to process
 */
void DataDeclarationSyntaxVisitor::_read_type_len(const slang::syntax::DataTypeSyntax* type)
{
    size_t current_type_size = 0;

    switch (type->kind)
    {
		case SyntaxKind::LogicType:
		{
			current_type_size += type_length(&(type->as<IntegerTypeSyntax>()));

			const auto &dimensions = type->as<IntegerTypeSyntax>().dimensions;

			dimension_syntax_to_vector(dimensions, _type_sizes);
		}
		break;

    default:
        // current_type_size += type->toString().length();
        //  In case of doubt, do nothing !
        break;
    }

    _type_name_size = std::max(_type_name_size, current_type_size);
}


/**
 * @brief If required, update the current block kind and process previous nodes
 * queued for formatting.
 * 
 * @param node new node which kind will be the new bloc type
 */
void DataDeclarationSyntaxVisitor::_switch_bloc_type(const SyntaxNode& node)
{
	if(_bloc_type_kind == SyntaxKind::Unknown)
		_bloc_type_kind = node.kind;

	if(! node.isKind(_bloc_type_kind))
		process_pending_formats();
}

void DataDeclarationSyntaxVisitor::_switch_bloc_type(const SyntaxNode* node)
{
	_switch_bloc_type(*node);
}

unsigned int type_length(const IntegerTypeSyntax* node)
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
 * @brief Given a list of tokens, push the size of each token in the target vector
 * 
 * @param tokens 
 * @param target_vector 
 */
void  tokens_to_vector(const std::span<const Token> tokens, std::vector<size_t>& target_vector)
{
	unsigned int index = 0;

	if (tokens.size() > target_vector.size())
		target_vector.resize(tokens.size(),0);

	for (const Token& tok : tokens)
	{
		if(! tok.valid())
			continue;
			
		target_vector[index] = std::max(target_vector[index],tok.rawText().length());
		index ++;
	}
}

/**
 * @brief Convert a syntax-list of variable dimensions to content size in a vector
 * 
 * @param dimensions Dimension to process
 * @param target_vector vector to handle and fill
 */
void dimension_syntax_to_vector(const SyntaxList<VariableDimensionSyntax> dimensions, std::vector<std::pair<size_t,size_t>>& target_vector)
{
	unsigned int index = 0;

	if (dimensions.size() > target_vector.size())
		target_vector.resize(dimensions.size(),{0,0});

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
					
					const size_t req_len = raw_text_from_syntax(*expr).length();
					const std::pair<size_t,size_t> nval = {req_len/2,req_len/2};

					target_vector[index].first = std::max(target_vector[index].first,nval.first);
					target_vector[index].second = std::max(target_vector[index].second,nval.second);
					index++;
				}
				break;
			case SyntaxKind::AscendingRangeSelect : [[fallthrough]]; 
			case SyntaxKind::DescendingRangeSelect: [[fallthrough]];
			case SyntaxKind::SimpleRangeSelect :
				{
					const auto& sel = specifier->as<RangeDimensionSpecifierSyntax>().selector->as<RangeSelectSyntax>();
					target_vector[index].first = std::max(target_vector[index].first, raw_text_from_syntax(*(sel.left.get())).length());
					target_vector[index].second = std::max(target_vector[index].second, raw_text_from_syntax(*(sel.right.get())).length());

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
	}
}

/**
 * @brief Get only text from a syntax node (without trivia)
 * 
 * @param node Node to print
 */
std::string raw_text_from_syntax(const SyntaxNode &node)
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
				if(trivia.kind == TriviaKind::Whitespace)
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

Token replace_spacing(slang::BumpAllocator& alloc , const Token& tok, const unsigned int space_number)
{
	// Store the string in "persistent" memory (BumpAllocator)
	if(space_number == 0)
		return tok.withTrivia(alloc,{});
	
	slang::byte* spacing_base = alloc.allocate(space_number, alignof(char));
	std::memset(spacing_base,' ',space_number);

	// Declare and store the trivia, then create the token to add.
	Trivia* t = alloc.emplace<Trivia>(TriviaKind::Whitespace,std::string_view((char*)spacing_base,space_number));
	return tok.withTrivia(alloc,{t,1});
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
	const unsigned int target_spacing = align_size > tok_len ? (align_size - tok_len) : (allow_no_space ? 0U : 1U);

	return replace_spacing(alloc,tok,target_spacing);
}



unsigned int align_dimension(slang::BumpAllocator& alloc,SyntaxList<VariableDimensionSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment)
{
	bool first_element = true; // Detect the first element to right-align
	assert(first_alignment >= 0);
	unsigned int size_dim_index = 0;
	unsigned int remaining_len = 0;
	if (dimensions.size() > 0)
	{
		for (auto* dim : dimensions)
		{
			if (first_element)
				dim->openBracket = token_align_right(alloc, dim->openBracket, first_alignment + 1, false);
			else
				dim->openBracket = replace_spacing(alloc, dim->openBracket, 0);

			switch (dim->specifier->kind)
			{
			case SyntaxKind::RangeDimensionSpecifier :
			{
				RangeDimensionSpecifierSyntax& spec = dim->specifier->as<RangeDimensionSpecifierSyntax>();
				if (spec.selector->kind == SyntaxKind::SimpleRangeSelect)
				{
					RangeSelectSyntax& select = spec.selector->as<RangeSelectSyntax>();
					unsigned int temp_size = select.left->toString().length();
					select.range = token_align_right(alloc, select.range, 1+ sizes_refs.at(size_dim_index).first - temp_size);

					temp_size = select.right->toString().length();
					dim->closeBracket = token_align_right(alloc, dim->closeBracket,1+ sizes_refs.at(size_dim_index).second - temp_size);

					size_dim_index ++;
				}
				else if (spec.selector->kind == SyntaxKind::BitSelect)
				{
					BitSelectSyntax& select = spec.selector->as<BitSelectSyntax>();
					unsigned int temp_size = select.expr->toString().length();
					const std::pair<size_t,size_t>& sizes = sizes_refs.at(size_dim_index++);
					dim->closeBracket = token_align_right(alloc, dim->closeBracket, 2+ sizes.first + sizes.second - temp_size);
				}
			}
				break;
			
			default:
				break;
			}

			first_element = false;
		}

	}
	else
	{
		// Compensate for the first bracket alignment if there is no bracket at all.
		remaining_len += first_alignment -1;	
	}

	// If all dimensions have not been used, compensate for missing dimensions with three characters ([:]) and 
	// appropriate sizes as mostly used.
	if(size_dim_index < sizes_refs.size())
	{
		remaining_len += (1 + 3 * (sizes_refs.size() - size_dim_index)) + sum_dim_vector(sizes_refs,size_dim_index);
	}


	return remaining_len;
}