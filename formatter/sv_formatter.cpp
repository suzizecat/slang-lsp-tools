#include "sv_formatter.hpp"
#include "fmt/format.h"
#include <iostream>

using namespace slang;

SVFormatter::SVFormatter():
	_indentation_level(0),
	_decl_name_width(0),
	_decl_type_width(0),
	_decl_sttm_width(0)
{}

std::string SVFormatter::_indent()
{
	std::string ret;
	for(int i = 0; i < _indentation_level; i++)
		ret += "\t";
	
	return ret;
}

void SVFormatter::_set_parameter_sttm_sizes(const syntax::ParameterPortListSyntax &node)
{
	_decl_name_width = 0;
	_decl_sttm_width = 0;
	_decl_type_width = 0;

	for(const syntax::ParameterDeclarationBaseSyntax* param : node.declarations)
	{
		syntax::DataTypeSyntax* param_type = (*param).as<syntax::ParameterDeclarationSyntax>().type;
		if( param_type->kind != syntax::SyntaxKind::ImplicitType)
		{
			_decl_type_width = std::max(_decl_type_width, param_type->toString().length());
		}
		for (const syntax::DeclaratorSyntax* decl : (*param).as<syntax::ParameterDeclarationSyntax>().declarators)
		{
			_decl_name_width = std::max(_decl_name_width,decl->name.rawText().length());
		}
	}
}

void SVFormatter::_set_parameter_sttm_sizes(const syntax::AnsiPortListSyntax &node)
{
	_decl_name_width = 0;
	_decl_sttm_width = 0;
	_decl_type_width = 0;

	for(const syntax::MemberSyntax* port : node.ports)
	{
		std::cout << "Port kind is " << syntax::toString(port->kind) << std::endl;
		syntax::PortHeaderSyntax* header = (*port).as<syntax::ImplicitAnsiPortSyntax>().header;
		syntax::DeclaratorSyntax* declaration = (*port).as<syntax::ImplicitAnsiPortSyntax>().declarator;
		std::cout << "  Header kind is " << syntax::toString(header->kind) << std::endl;
		std::cout << "  Header is " << header->toString() << std::endl;
		std::cout << "  Declar is " << declaration->toString() << std::endl;

		for(const parsing::Trivia t : header->getFirstToken().trivia())
		{
			if(t.getRawText().starts_with("//"))
				std::cout << "     - Comment is" << t.getRawText() << std::endl;
		}
		/*
		if( param_type->kind != syntax::SyntaxKind::ImplicitType)
		{
			_decl_type_width = std::max(_decl_type_width, param_type->toString().length());
		}
		for (const syntax::DeclaratorSyntax* decl : (*param).as<syntax::ParameterDeclarationSyntax>().declarators)
		{
			_decl_name_width = std::max(_decl_name_width,decl->name.rawText().length());
		}
		*/

		_decl_name_width = std::max(_decl_name_width,declaration->name.rawText().length());
		
	}

		std::cout << "     - Last token kind is " << parsing::toString(node.getLastToken().kind) << std::endl;
		for(const parsing::Trivia t : node.getLastToken().trivia())
		{
			if(t.getRawText().starts_with("//"))
				std::cout << "     - Top comment is" << t.getRawText() << std::endl;
		}
}

void SVFormatter::handle(const syntax::ModuleDeclarationSyntax& node)
{
	// std::cout << _indent() << "Enter " << syntax::toString(node.kind) <<std::endl;
	visitDefault(node);
	_indentation_level --;
	_content += fmt::format("{}endmodule\n",_indent());
}

void SVFormatter::handle(const syntax::ModuleHeaderSyntax& node)
{
	// std::cout << _indent() << "Enter " << syntax::toString(node.kind) <<std::endl;
	std::string_view module_name = node.name.valueText();
	_content += fmt::format("{}module {}",_indent(), module_name);

	visitDefault(node);
	_indentation_level ++;
}

void SVFormatter::handle(const syntax::ParameterPortListSyntax& node)
{
	// std::cout << _indent() << "Enter " << syntax::toString(node.kind) <<std::endl;
	_param_declarations.clear();

	if(node.declarations.size() > 0)
	{
		_set_parameter_sttm_sizes(node);
		_content += " #(\n";
		_indentation_level ++;
		visitDefault(node);
		for(const std::string& sttm : _param_declarations)
		{
			_content += fmt::format("{}{:{}s},\n",_indent(),sttm,_decl_sttm_width);
		}
		_indentation_level --;
		
		if(! _content.empty())
		{
			// Removes the last coma without removing the \n
			_content.erase(_content.length()-2,1);
		}
		_content += fmt::format("{}) ",_indent());
	}    
}

void SVFormatter::handle(const syntax::ParameterDeclarationSyntax& node)
{
	std::string curr_sttm;

	for(const syntax::DeclaratorSyntax* decl : node.declarators)
	{
		curr_sttm = "parameter ";

		if( node.type->kind != syntax::SyntaxKind::ImplicitType)
		{
			curr_sttm += fmt::format("{:{}s} ",node.type->toString(),_decl_type_width);
		}
		else if (_decl_type_width != 0)
		{
			curr_sttm += fmt::format("{:{}s} "," ",_decl_type_width);
		}
		
		curr_sttm += fmt::format("{:{}s}",decl->name.rawText(),_decl_name_width);
		if(decl->initializer != nullptr)
		{
			curr_sttm += fmt::format(" = {}",decl->initializer->expr->toString());
		}
		_decl_sttm_width = std::max(_decl_sttm_width,curr_sttm.length());
		_param_declarations.push_back(curr_sttm);
	}
}


void SVFormatter::handle(const syntax::AnsiPortListSyntax& node)
{
	// std::cout << _indent() << "Enter " << syntax::toString(node.kind) <<std::endl;
	_param_declarations.clear();
	_content += "(";

	if(node.ports.size() > 0)
	{
		_set_parameter_sttm_sizes(node);
		/*_content += " #(\n";
		_indentation_level ++;
		visitDefault(node);
		for(const std::string& sttm : _param_declarations)
		{
			_content += fmt::format("{}{:{}s},\n",_indent(),sttm,_decl_sttm_width);
		}
		_indentation_level --;
		
		if(! _content.empty())
		{
			// Removes the last coma without removing the \n
			_content.erase(_content.length()-2,1);
		}
		_content += fmt::format("{}) ",_indent());
		*/
	}    
}
