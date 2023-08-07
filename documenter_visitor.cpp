#include "documenter_visitor.h"
#include "slang/text/SourceManager.h"
#include <iostream>

using namespace slang::syntax;


DocumenterVisitor::DocumenterVisitor(bool only_modules, const slang::SourceManager* sm) : _only_modules(only_modules), _sm(sm) {}

void DocumenterVisitor::set_source_manager(const slang::SourceManager* new_sm)
{
	_sm = new_sm;
}

void DocumenterVisitor::handle(slang::syntax::ModuleHeaderSyntax& node)
{
    // std::cout << "Entering module " << node.name.valueText() << std::endl;
	module["name"] = node.name.valueText();

	module["file"] = _sm == nullptr ? "" : _sm->getFileName(node.sourceRange().start());

	if(! _only_modules)
	{
		data["module"] = node.name.valueText();
		data["ports"] = json::array();
		data["parameters"] = json::array();
		visitDefault(node);
	}
}

void DocumenterVisitor::handle(NonAnsiPortSyntax& port)
{
    // std::cout << "Found NAP  " << port.toString() << std::endl;
}

void DocumenterVisitor::handle(NonAnsiPortListSyntax& port)
{
    // std::cout << "Found NAPL " << port.toString() << std::endl;
}

void DocumenterVisitor::handle(ImplicitAnsiPortSyntax& port)
{
	json json_def = json::object();
	// std::cout << "Found ImplicitAnsiPort " << std::endl;
	const DeclaratorSyntax *declarator = port.declarator;

	// std::cout << "    Name      : " << declarator->name.valueText() << std::endl;
	json_def["name"] = declarator->name.valueText();

	switch (port.header->kind)
	{
	case SyntaxKind::VariablePortHeader:
		{
			VariablePortHeaderSyntax& header = port.header->as<VariablePortHeaderSyntax>();


			// std::cout << "    Direction : " << header.direction.valueText() << std::endl;
			IntegerTypeSyntax* port_type = header.dataType->as_if<IntegerTypeSyntax>();
			if(port_type == nullptr)
			{
				json_def["type"] = "unknown";
				json_def["kind"] = toString(header.dataType->kind);
			}
			else
			{
				// std::cout << "    Data type : " << port_type.keyword.valueText() << std::endl;

				json_def["direction"] = header.direction.valueText();
				json_def["type"] = port_type->keyword.valueText();



				if(port_type->dimensions.getChildCount() == 0)
				{
					// std::cout << "    Data size : 1" << std::endl;
					json_def["size"] = 1;
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
					json_def["size"] = size_expr;
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

			json_def["interface"] = header.nameOrKeyword.valueText();
			json_def["modport"] = header.modport->member.valueText();

		}
		break;
	default:
			// std::cout << "    Unhandled port header kind " << port.header->kind << " for " << port.toString() << std::endl;
			break;
	}

	data["ports"].push_back(json_def);
}

void DocumenterVisitor::handle(ParameterDeclarationSyntax& node)
{
    json param_def = json();
	
	DataTypeSyntax* param_type = node.type;

	if(node.keyword.valueText() == "localparam")
		return ;

	param_def["type"] = toString(param_type->kind);

	for (const DeclaratorSyntax* decl : node.declarators)
	{
			param_def["name"] = decl->name.valueText();
			if(decl->initializer != nullptr)
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

				param_def["default"] = init_expr;
			}
			data["parameters"].push_back(param_def);

	}
}