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
		ret += self.json_binding(idt)

		for ns in namespace :
			idt.sub_indent_level()
			ret += f"{idt}}}; // namespace {ns} \n"

		return ret

	def json_binding(self,idt:IndentHandler = None):
		if idt is None:
			idt = IndentHandler(base_indent="\t")
		ret = self._to_json(idt)
		ret += self._from_json(idt)
		return ret

	def _to_json(self, idt:IndentHandler):
		ret = f"{idt}void to_json(json& j, const {self.type.type_name}& s) {{\n"
		idt.add_indent_level()
		ret += f"{idt}j = json();\n"
		ret +=  self._to_json_bindings(idt)
		idt.sub_indent_level()
		ret += f"{idt}}}\n"
		return ret

	def _from_json(self, idt:IndentHandler):
		ret = f"{idt}void from_json(const json& j, {self.type.type_name}& s) {{\n"
		idt.add_indent_level()
		ret +=  self._from_json_bindings(idt)
		idt.sub_indent_level()
		ret += f"{idt}}}\n"
		return ret

	def _to_json_bindings(self, idt):
		jsbinding = ""
		for p in self.properties:
			if p.type.is_def_optional:
				if p.type.is_optional:
					jsbinding += f'{idt}if (s.{p.name})\n{idt + 1} j["{p.name}"] = s.{p.name}.value();\n\n'
				elif p.type.is_nullable:
					jsbinding += f'{idt}j["{p.name}"] = s.{p.name} ? s.{p.name}.value() : nullptr;\n'
			else:
				jsbinding += f'{idt}j["{p.name}"] = s.{p.name};\n'
		return jsbinding

	def _from_json_bindings(self, idt):
		jsbinding = ""
		for p in self.properties:
			add_level = 0
			if p.type.is_def_optional:
				if p.type.is_optional:
					jsbinding += f'{idt}if (j.contains("{p.name}"))\n'
					add_level += 1
				if p.type.is_nullable:
					jsbinding += f'{idt + add_level}if (! j["{p.name}"].is_null()))\n'
					add_level += 1
			jsbinding += f'{idt+add_level}j.at("{p.name}").get_to(s.{p.name});\n'
			if p.type.is_def_optional :
				jsbinding += "\n"
		return jsbinding

	def get_include_list(self):
		ret = list()
		multi = set()
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










