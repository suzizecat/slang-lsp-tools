#include "hier_visitor.h"
#include <iostream>
using json = nlohmann::json;
namespace ast = slang::ast;

 HierVisitor::HierVisitor(bool output_io) : _output_io(output_io)
 {
	_hierarchy = json::array();
 }

void HierVisitor::handle(const slang::ast::InstanceSymbol &node)
{
	//_hierarchy[_pointer].push_back(json());
	_pointer.push_back(std::to_string(_hierarchy[_pointer].size()));
	if(_output_io)
	{
		_hierarchy[_pointer/"in"] = json::array();
		_hierarchy[_pointer/"out"] = json::array();
	}
	_hierarchy[_pointer/"def"] = true;
	_hierarchy[_pointer/"name"] = std::string(node.name);
	_hierarchy[_pointer/"childs"] = json::array();
	_pointer.push_back("childs");
	visitDefault(node);
	_pointer.pop_back();
	_pointer.pop_back();
}

void HierVisitor::handle(const slang::ast::PortSymbol& node)
{
	if(_output_io)
	{
		if(node.direction == ast::ArgumentDirection::In)
		{
			_hierarchy[_pointer/"in"].push_back(node.name);
		}
		else if(node.direction == ast::ArgumentDirection::Out)
		{
			_hierarchy[_pointer/"out"].push_back(node.name);
		}
	}
	//visitDefault(node);
}

void HierVisitor::handle(const slang::ast::UninstantiatedDefSymbol& node)
{
	//_hierarchy[_pointer].push_back(json());
	_pointer.push_back(std::to_string(_hierarchy[_pointer].size()));
	_hierarchy[_pointer/"def"] = false;
	_hierarchy[_pointer/"name"] = node.name;
	_pointer.pop_back();
}

/*
void HierVisitor::handle(const slang::ast::ValueSymbol& node)
{
	std::cout << "Value symbol " << node.name << " of kind "<< ast::toString(node.kind) << " driven by " << std::endl;
	for(const auto* drv : node.drivers())
	{
		std::cout << "    " << ast::toString(drv->kind) << " "  << drv->containingSymbol->name << " (" << ast::toString(drv->containingSymbol->kind) << ") " << std::endl;
	}
	visitDefault(node);
}
*/