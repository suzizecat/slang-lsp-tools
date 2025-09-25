import typing as _T
from lspgen.indentation import IndentHandler
from lspgen.LSPTyped import LSPTypedBase
from lspgen.lsp_type import LSPType
from lspgen.lsp_property import LSPProperty


class LSPDefinition(LSPTypedBase):
	def __init__(self,name):
		super().__init__(name)
		self.extend_list : _T.List[LSPType] = list()
		self.properties : _T.List[LSPProperty] = list()

		# List of definitions found when resolving the extends list.
		# Particularly used for the to/from json operations.
		self.extend_definitions : _T.List[LSPDefinition] = list()
		self.documentation : str = ""



	def as_cpp(self, idt: IndentHandler = None):
		if idt is None :
			idt = IndentHandler()

		ret = ""
		ret += self._doc.as_cpp(idt)

		ret += f"{idt}typedef struct {self.cpp_type} "
		if len(self.extend_list) > 0 :
			ret += " : " + ", ".join(["public " + str(t) for t in self.extend_list])
		ret += f"\n{idt}{{\n"
		idt.add_indent_level()
		for p in self.properties :
			ret += p.as_cpp(idt)
			ret += f"\n"

		idt.sub_indent_level()
		ret += f"{idt}}}  {self.cpp_type};\n\n"
		return ret

	def as_cpp_file(self,idt:IndentHandler = None, namespace : _T.List[str] = None ):
		if namespace is None :
			namespace = list()
		if idt is None:
			idt = IndentHandler(base_indent="\t")

		ret = f"{idt}#pragma once\n\n"
		for inc in self.get_include_list() :
			ret += f"{idt}#include {inc}\n"

		for ns in namespace :
			ret += f"{idt}namespace {ns} {{\n"
			idt.add_indent_level()

		ret += self.as_cpp(idt)
		ret += self.json_bind_prototypes(idt)

		for ns in namespace :
			idt.sub_indent_level()
			ret += f"{idt}}}; // namespace {ns} \n"

		return ret

	def json_bind_prototypes(self,idt : IndentHandler = None):
		if idt is None:
			idt = IndentHandler()
		ret = (f"{idt}void to_json(nlohmann::json& j, const {self.type.type_name}& s);\n"
			   f"{idt}void from_json(const nlohmann::json& j, {self.type.type_name}& s);\n")
		return ret

	def json_binding(self,idt:IndentHandler = None):
		if idt is None:
			idt = IndentHandler()
		ret = self._to_json(idt)
		ret += self._from_json(idt)
		return ret

	def _to_json(self, idt:IndentHandler):
		ret = f"{idt}void to_json(nlohmann::json& j, const {self.type.type_name}& s) {{\n"
		idt.add_indent_level()
		ret += f"{idt}j = nlohmann::json();\n"
		ret +=  self._to_json_bindings(idt)

		defined_properties_names = [x.name for x in self.properties]
		for ext in self.extend_definitions :
			ret += f"{idt}// Bindings inherited from {ext.type.type_name}\n"
			ret += ext._to_json_bindings(idt, defined_properties_names)
		idt.sub_indent_level()
		ret += f"{idt}}}\n"
		return ret

	def _from_json(self, idt:IndentHandler):
		ret = f"{idt}void from_json(const nlohmann::json& j, {self.type.type_name}& s) {{\n"
		idt.add_indent_level()
		ret +=  self._from_json_bindings(idt)

		defined_properties_names = [x.name for x in self.properties]

		for ext in self.extend_definitions :
			ret += f"{idt}// Bindings inherited from {ext.type.type_name}\n"
			ret += ext._from_json_bindings(idt,defined_properties_names)
		idt.sub_indent_level()
		ret += f"{idt}}}\n"
		return ret

	def _to_json_bindings(self, idt, exclude_list : _T.List[str] = None):
		jsbinding = ""
		if exclude_list is None :
			exclude_list = []

		for p in self.properties:
			
			if p.name in exclude_list :
				continue

			jsbinding += p.type.to_json(idt,p.name)
		return jsbinding

	def _from_json_bindings(self, idt, exclude_list : _T.List[str] = None):
		jsbinding = ""
		if exclude_list is None :
			exclude_list = []
			
		for p in self.properties:
			if p.name in exclude_list :
				continue
			if p.literal_value is not None : 
				continue
			jsbinding += p.type.from_json(idt,p.name)
		return jsbinding

	def get_include_list(self):
		ret = list()
		multi = set()
		multi.add('"nlohmann/json.hpp"')
		got_optional = False
		got_array = False
		for p in self.properties :
			if p.type.is_def_optional and not got_optional:
				ret.append("<optional>")
				got_optional = True
			if p.type.is_array and not got_array:
				ret.append("<vector>")
				got_array = True

			if p.type.related_include is not None and p.type.related_include != "" :
				multi.add(p.type.related_include)

		for ext in self.extend_list :
			multi.add(ext.related_include)

		for t in multi :
			ret.append(t)

		ret.sort()
		return ret










