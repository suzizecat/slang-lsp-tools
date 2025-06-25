#include "hier_visitor.h"
#include <slang/text/SourceManager.h>
#include <iostream>
using json = nlohmann::json;
namespace ast = slang::ast;

 HierVisitor::HierVisitor(bool output_io, const std::unordered_map<std::filesystem::path, uri>* path_to_uri) : _output_io(output_io), _doc_path_to_uri(path_to_uri)
 {
	_hierarchy = json::array();
 }

void HierVisitor::handle(const slang::ast::InstanceSymbol &node)
{
	//_hierarchy[_pointer].push_back(json());
	
	_pointer.push_back(std::to_string(_hierarchy[_pointer].size()));

	const ast::DefinitionSymbol& def = node.getDefinition();
	const slang::SourceManager* sm = def.getParentScope()->getCompilation().getSourceManager();

	if (_output_io)
	{
		_hierarchy[_pointer/"in"] = json::array();
		_hierarchy[_pointer/"out"] = json::array();
	}
	_hierarchy[_pointer/"def"] = true;
	_hierarchy[_pointer/"name"] = std::string(node.name);
	_hierarchy[_pointer/"module"] = std::string(def.name);
	_hierarchy[_pointer / "childs"] = json::array();

	const std::filesystem::path& filepath = sm->getFullPath(def.location.buffer());

	if (_doc_path_to_uri != nullptr && _doc_path_to_uri->contains(filepath))
	{
		_hierarchy[_pointer / "file"] = _doc_path_to_uri->at(filepath).to_string();	
	}
	else
	{
		_hierarchy[_pointer / "file"] = filepath.generic_string();
	}

	
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
	_hierarchy[_pointer/"module"] = std::string(node.definitionName);
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
