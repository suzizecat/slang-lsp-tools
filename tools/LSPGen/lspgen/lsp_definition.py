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

	def legalize(self):
		self.documentation.strip()

	def as_cpp(self, idt: IndentHandler = None):
		self.legalize()
		if idt is None :
			idt = IndentHandler(base_indent="\t")

		ret = ""
		if len(self.documentation) > 0:
			ret += f"{idt}/**\n"
			for line in self.documentation.split("\n") :
				ret += f"{idt} * {line}\n"
			ret += f"{idt} */\n"

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
		self.legalize()
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

		for ns in namespace :
			ret += f"{idt}}}; // namespace {ns} \n"
			idt.sub_indent_level()

		return ret

	def get_include_list(self):
		ret = list()
		multi = set()
		got_optional = False
		got_array = False
		for p in self.properties :
			if p.type.is_optional and not got_optional:
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










