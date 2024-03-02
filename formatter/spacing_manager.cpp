#include "spacing_manager.hpp"

#include "formatter_utils.hpp"

using namespace slang::syntax;
using namespace slang::parsing;

SpacingManager::SpacingManager(slang::BumpAllocator& alloc, const unsigned int space_per_level, const bool use_tabs ) :
    _level(0),
    _space_per_level(space_per_level),
    _use_tabs(use_tabs),
    _mem(alloc)
{}

slang::parsing::Token SpacingManager::replace_spacing(const slang::parsing::Token &tok, int spaces)
{	
	assert(spaces >= 0);
	// Store the string in "persistent" memory (BumpAllocator)
	if(spaces == 0)
		return tok.withTrivia(_mem,{});
	
	slang::byte* spacing_base = _mem.allocate(spaces, alignof(char));
	std::memset(spacing_base,' ',spaces);

	// Declare and store the trivia, then create the token to add.
	Trivia* t = _mem.emplace<Trivia>(TriviaKind::Whitespace,std::string_view((char*)spacing_base,spaces));
	return tok.withTrivia(_mem,{t,1});
}

slang::parsing::Token SpacingManager::remove_spacing(const slang::parsing::Token &tok)
{
	return replace_spacing(tok, 0);
}

slang::parsing::Token SpacingManager::indent(const slang::parsing::Token& tok, int additional_spacing)
{

	assert(additional_spacing >= 0);
	// Store the string in "persistent" memory (BumpAllocator)
	unsigned int mem_indent = _level * (_use_tabs ? 1 : _space_per_level);
	unsigned int mem_full = mem_indent + additional_spacing;
	slang::byte* spacing_base = _mem.allocate(mem_full, alignof(char));
	
	std::memset(spacing_base,_use_tabs ? '\t' : ' ',mem_indent);
	std::memset(spacing_base + mem_indent ,' ',additional_spacing);

	Trivia indent_trivia(TriviaKind::Whitespace,std::string_view((char*)spacing_base,mem_full));

	slang::byte* newline = _mem.allocate(1, alignof(char));
	*newline = (slang::byte)'\n';
	Trivia newline_trivia(TriviaKind::EndOfLine,std::string_view((char*)newline,1));
	
	// The goal is to skip all whitespaces at line start to replace them by the indent
	slang::SmallVector<Trivia> kept_trivia;
	
	bool skip_spacings = false;
	bool newline_clean  = false;

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
			{
				skip_spacings = true;
				newline_clean = true;
			}
			else
			{
				skip_spacings = false;
				newline_clean = false;
			}
			kept_trivia.push_back(trivia);

			if(trivia.kind == TriviaKind::EndOfLine)
				kept_trivia.push_back(indent_trivia);
		}
	}
	
	if(! newline_clean)
	{
		kept_trivia.push_back(newline_trivia);
		kept_trivia.push_back(indent_trivia);
	}

	// Declare and store the indentation trivia, then create the token to add.
	return tok.withTrivia(_mem,kept_trivia.copy(_mem));
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
Token SpacingManager::token_align_right(const Token& tok, const unsigned int align_size, const bool allow_no_space )
{
	const unsigned int tok_len = tok.rawText().length();
	const unsigned int target_spacing = align_size > tok_len ? (align_size - tok_len) : (allow_no_space ? 0U : 1U);

	return replace_spacing(tok,target_spacing);
}



unsigned int SpacingManager::align_dimension(SyntaxList<VariableDimensionSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment)
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
				dim->openBracket = token_align_right(dim->openBracket, first_alignment + 1, false);
			else
				dim->openBracket = replace_spacing(dim->openBracket, 0);

			switch (dim->specifier->kind)
			{
			case SyntaxKind::RangeDimensionSpecifier :
			{
				const std::pair<size_t,size_t>& sizes = sizes_refs.at(size_dim_index++);
				dim->closeBracket = remove_spacing(dim->closeBracket);

				RangeDimensionSpecifierSyntax& spec = dim->specifier->as<RangeDimensionSpecifierSyntax>();
				if (spec.selector->kind == SyntaxKind::SimpleRangeSelect)
				{
					RangeSelectSyntax& select = spec.selector->as<RangeSelectSyntax>();
					Token* to_edit;
					to_edit = select.left->getFirstTokenPtr();

					unsigned int temp_size = raw_text_from_syntax(select.left).length() - to_edit->rawText().size();

					*to_edit = token_align_right(*to_edit, sizes.first - temp_size);

					to_edit = select.right->getFirstTokenPtr();
					temp_size = raw_text_from_syntax(select.right).length() - to_edit->rawText().size();
					*to_edit = token_align_right(*to_edit, sizes.second - temp_size);

					select.range = remove_spacing(select.range);
				}
				else if (spec.selector->kind == SyntaxKind::BitSelect)
				{
					BitSelectSyntax& select = spec.selector->as<BitSelectSyntax>();

					unsigned int temp_size = raw_text_from_syntax(*(select.expr)).length();
					
					Token* to_edit = select.expr->getFirstTokenPtr();
					temp_size -= to_edit->rawText().size();
					*to_edit = token_align_right(*to_edit,1+ sizes.first + sizes.second - temp_size);

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


unsigned int SpacingManager::align_dimension(SyntaxList<ElementSelectSyntax>& dimensions,const std::vector<std::pair<size_t,size_t>>& sizes_refs, const int first_alignment)
{
	bool first_element = true; // Detect the first element to right-align
	assert(first_alignment >= 0);
	unsigned int size_dim_index = 0;
	unsigned int remaining_len = 0;
	unsigned int temp_size = 0;
	Token* to_edit;
	if (dimensions.size() > 0)
	{
		for (auto* dim : dimensions)
		{
			if (first_element)
				dim->openBracket = token_align_right(dim->openBracket, first_alignment + 1, false);
			else
				dim->openBracket = remove_spacing(dim->openBracket);

			const std::pair<size_t,size_t>& sizes = sizes_refs.at(size_dim_index++);

			switch (dim->selector->kind)
			{
				

				case SyntaxKind::SimpleRangeSelect :
				{
					RangeSelectSyntax& select = dim->selector->as<RangeSelectSyntax>();
					
					to_edit = select.left->getFirstTokenPtr();				
					temp_size = raw_text_from_syntax(select.left).length() - to_edit->rawText().size();
					*to_edit = token_align_right(*to_edit, sizes.first - temp_size);

					to_edit = select.right->getFirstTokenPtr();
					temp_size = raw_text_from_syntax(select.right).length() - to_edit->rawText().size();
					*to_edit = token_align_right(*to_edit, sizes.second - temp_size);

					select.range = remove_spacing(select.range);
					dim->closeBracket = remove_spacing(dim->closeBracket);
				}
				break;
				case SyntaxKind::BitSelect :
				{
					BitSelectSyntax& select = dim->selector->as<BitSelectSyntax>();
					to_edit = select.expr->getFirstTokenPtr();	
					temp_size = raw_text_from_syntax(select.expr).length() - to_edit->rawText().size();
					*to_edit = token_align_right(*to_edit, 1+ sizes.first + sizes.second - temp_size);

					dim->closeBracket = remove_spacing(dim->closeBracket);
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

IndentLock::IndentLock(SpacingManager& manager, unsigned int level_to_add) :
	_managed(manager),
	_added_level(level_to_add)
{
	_managed.add_level(_added_level);
}

IndentLock::~IndentLock()
{
	_managed.sub_level(_added_level);
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

std::string raw_text_from_syntax(const SyntaxNode *node)
{
	return raw_text_from_syntax(*node);
}