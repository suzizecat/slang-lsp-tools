#include "visitor_module_bb.hpp"
#include "slang/text/SourceManager.h"
#include "slang/parsing/Token.h"
#include "slang/parsing/TokenKind.h"
#include <iostream>
#include <bit>
#include "spdlog/spdlog.h"


using namespace slang::syntax;


void to_json(json& j, const ModuleParam& p)
{
	j = json{
			{"name",p.name},
			{"default",p.default_value},
			{"type",p.type}
		};
}
void to_json(json& j, const ModulePort& p)
{
	j = json{
		{"name",p.name},
		{"size",p.size},
		{"type",p.type},
		{"direction",p.direction},
		{"is_interface",p.is_interface},
		{"modport",p.modport},
		{"comment",p.comment}
	};
}
void to_json(json& j, const ModuleBlackBox& p)
{
	j = json{
		{"module",p.module_name},
		{"parameters",p.parameters},
		{"ports",p.ports}
	};
}

void to_json(json& j, const  ModuleBlackBox* const & p)
{
	j = json{
		{"module",p->module_name},
		{"parameters",p->parameters},
		{"ports",p->ports}
	};
}

void from_json(const json& j, ModuleParam& p)
{
	j.at("name").get_to(p.name);
	j.at("default").get_to(p.default_value);
	j.at("type").get_to(p.type);
}
void from_json(const json& j, ModulePort& p)
{
	j.at("name").get_to(p.name);
	j.at("size").get_to(p.size);
	j.at("type").get_to(p.type);
	j.at("direction").get_to(p.direction);
	j.at("is_interface").get_to(p.is_interface);
	j.at("modport").get_to(p.modport);
	j.at("comment").get_to(p.comment);
}
void from_json(const json& j, ModuleBlackBox& p)
{
	j.at("module").get_to(p.module_name);
	j.at("parameters").get_to(p.parameters);
	j.at("ports").get_to(p.ports);
}

VisitorModuleBlackBox::VisitorModuleBlackBox(bool only_modules, const slang::SourceManager* sm) : 
	_only_modules(true), 
	_bb(new ModuleBlackBox()),
	_sm(sm),
	read_bb(new std::unordered_map<std::string, std::unique_ptr<ModuleBlackBox> >()) 
{

}

void VisitorModuleBlackBox::set_source_manager(const slang::SourceManager* new_sm)
{
	_sm = new_sm;
}

void VisitorModuleBlackBox::handle(const slang::syntax::ModuleDeclarationSyntax& node)
{
	_bb.reset(new ModuleBlackBox());
	visitDefault(node);
	// Save the new BB in the output buffer.
	read_bb->emplace(_bb->module_name, std::move(_bb));
}

void VisitorModuleBlackBox::handle(const slang::syntax::ModuleHeaderSyntax& node)
{
	_bb->module_name = node.name.valueText();

	visitDefault(node);
}

void VisitorModuleBlackBox::handle(const HierarchyInstantiationSyntax& node)
{
	_bb->deps.insert(std::string(node.type.rawText()));
}


void VisitorModuleBlackBox::handle(const AnsiPortListSyntax& node)
{
	visitDefault(node);

	if(_bb->ports.size() > 0)
	{
		for(const slang::parsing::Trivia t : node.getLastToken().trivia())
		{
			if(t.getRawText().starts_with("//"))
			{
				_bb->ports.back().comment = std::string(t.getRawText());
			}
		}
	}
}

void VisitorModuleBlackBox::handle(const ImplicitAnsiPortSyntax& port)
{

	json json_def = json::object();
	ModulePort mport;
	// std::cout << "Found ImplicitAnsiPort " << std::endl;
	const DeclaratorSyntax *declarator = port.declarator;

	// std::cout << "    Name      : " << declarator->name.valueText() << std::endl;
	mport.name = declarator->name.valueText();
	// _bb->ports hold the list of ports of the module I'm analyzing
	if(_bb->ports.size() > 0)
	{
		for(const slang::parsing::Trivia t : port.header->getFirstToken().trivia())
		{
			// Assuming that I will only have one comment, could be refined.
			// in particular by checking the line number with the port declaration.
			if(t.getRawText().starts_with("//"))
			{
				_bb->ports.back().comment = std::string(t.getRawText());
				break;
			}
		}
	}

	switch (port.header->kind)
	{
	case SyntaxKind::VariablePortHeader:
		{
			VariablePortHeaderSyntax& header = port.header->as<VariablePortHeaderSyntax>();


			// std::cout << "    Direction : " << header.direction.valueText() << std::endl;
			IntegerTypeSyntax* port_type = header.dataType->as_if<IntegerTypeSyntax>();
			mport.type = "logic";
			if (port_type == nullptr)
			{
				// json_def["type"] = "unknown";
				// json_def["kind"] = toString(header.dataType->kind);
			}
			else
			{
				// std::cout << "    Data type : " << port_type.keyword.valueText() << std::endl;

				mport.direction = header.direction.valueText();
				mport.type = port_type->keyword.valueText();



				if(port_type->dimensions.getChildCount() == 0)
				{
					// std::cout << "    Data size : 1" << std::endl;
					mport.size = "1";
				}
				else
				{
					// std::cout << "    Data size : ";
					std::string size_expr;
					for (const VariableDimensionSyntax *dimen : port_type->dimensions)
					{
						slang::parsing::Token tok;
						SyntaxNode* node;
						for (int i = 0; i < dimen->getChildCount(); i++)
						{
							const ConstTokenOrSyntax child = dimen->getChild(i);
							if(child.isToken())
							{
								size_expr +=  child.token().valueText();
							}
							// The node may be null if there is a weird syntax issue such as logic [<empty>] ...
							// Issue #15
							else if (child.isNode() && child.node() != nullptr)
							{
								size_expr +=  child.node()->toString();
							}
						}
					}
					mport.size = size_expr;
					// json_def["size"] = size_expr;
					// std::cout << size_expr<< std::endl;
				}
			}
		}
		break;
	
	case SyntaxKind::InterfacePortHeader:
		{
			InterfacePortHeaderSyntax &header = port.header->as<InterfacePortHeaderSyntax>();
			
			mport.is_interface = true;
			mport.type = header.nameOrKeyword.valueText();
			mport.modport = header.modport->member.valueText();

		}
		break;
	default:
			// std::cout << "    Unhandled port header kind " << port.header->kind << " for " << port.toString() << std::endl;
			break;
	}

	//data["ports"].push_back(json_def);
	_bb->ports.push_back(mport);

}

void VisitorModuleBlackBox::handle(const ParameterDeclarationSyntax& node)
{
	ModuleParam mparam;
	
	//json param_def = json();
	
	DataTypeSyntax* param_type = node.type;

	if(node.keyword.valueText() == "localparam")
		return ;

	//param_def["type"] = toString(param_type->kind);
	mparam.type = toString(param_type->kind);

	for (const DeclaratorSyntax* decl : node.declarators)
	{
		//param_def["name"] = decl->name.valueText();
		mparam.name = decl->name.valueText();
		if (decl->initializer != nullptr)
		{

			std::string init_expr;
			for (int i = 0; i < decl->initializer->expr->getChildCount(); i++)
			{
				SyntaxNode* child = decl->initializer->expr->childNode(i);
				if(child == nullptr)
					init_expr +=  decl->initializer->expr->childToken(i).valueText();
				else
					init_expr +=  child->toString();
			}

			mparam.default_value = init_expr;
		}
		
		_bb->parameters.push_back(mparam);

	}
}

std::size_t bb_signature(const ModuleBlackBox& bb)
{
	return bb_signature(bb.module_name, 
		bb.parameters | std::views::transform([](const ModuleParam& p){return p.name;}), 
		bb.ports | std::views::transform([](const ModulePort& p){return p.name;}));
}
