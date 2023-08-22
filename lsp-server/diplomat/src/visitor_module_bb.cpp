#include "visitor_module_bb.hpp"
#include "slang/text/SourceManager.h"
#include <iostream>

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
		{"modport",p.modport}
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
}
void from_json(const json& j, ModuleBlackBox& p)
{
	j.at("module").get_to(p.module_name);
	j.at("parameters").get_to(p.parameters);
	j.at("ports").get_to(p.ports);
}

VisitorModuleBlackBox::VisitorModuleBlackBox(bool only_modules, const slang::SourceManager* sm) : _only_modules(true), _sm(sm) {}

void VisitorModuleBlackBox::set_source_manager(const slang::SourceManager* new_sm)
{
	_sm = new_sm;
}

void VisitorModuleBlackBox::handle(slang::syntax::ModuleHeaderSyntax& node)
{
    // std::cout << "Entering module " << node.name.valueText() << std::endl;
	// module["name"] = node.name.valueText();
	//module_name = node.name.valueText();
	bb.module_name = node.name.valueText();

	//module["file"] = _sm == nullptr ? "" : _sm->getFileName(node.sourceRange().start());

	// if(! _only_modules)
	// {
	// 	data["module"] = node.name.valueText();
	// 	data["ports"] = json::array();
	// 	data["parameters"] = json::array();
	visitDefault(node);
	// }
}

void VisitorModuleBlackBox::handle(NonAnsiPortSyntax& port)
{
    // std::cout << "Found NAP  " << port.toString() << std::endl;
}

void VisitorModuleBlackBox::handle(NonAnsiPortListSyntax& port)
{
    // std::cout << "Found NAPL " << port.toString() << std::endl;
}

void VisitorModuleBlackBox::handle(ImplicitAnsiPortSyntax& port)
{

	json json_def = json::object();
	ModulePort mport;
	// std::cout << "Found ImplicitAnsiPort " << std::endl;
	const DeclaratorSyntax *declarator = port.declarator;

	// std::cout << "    Name      : " << declarator->name.valueText() << std::endl;
	mport.name = declarator->name.valueText();

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
								size_expr +=  child.token().valueText();
							else if (child.isNode())
								size_expr +=  child.node()->toString();
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
			// std::cout << "    Interface : " << header.nameOrKeyword.valueText() << std::endl;
			// std::cout << "    Modport   : " << header.modport->member.valueText() << std::endl;
			mport.is_interface = true;
			mport.type = header.nameOrKeyword.valueText();
			mport.modport = header.modport->member.valueText();
			// json_def["interface"] = header.nameOrKeyword.valueText();
			// json_def["modport"] = header.modport->member.valueText();

		}
		break;
	default:
			// std::cout << "    Unhandled port header kind " << port.header->kind << " for " << port.toString() << std::endl;
			break;
	}

	//data["ports"].push_back(json_def);
	bb.ports.push_back(mport);

}

void VisitorModuleBlackBox::handle(ParameterDeclarationSyntax& node)
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
		
		bb.parameters.push_back(mparam);

	}
}