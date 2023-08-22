import typing as T
from lspgen.indentation import IndentHandler
class LSPMethod:
	def __init__(self, name):
		self.name = name

	def __str__(self):
		return self.name

class LSPMethodCollection:
	def __init__(self):
		self.content = list()


	def __contains__(self, item):
		return item in self.content

	def append(self,method : LSPMethod):
		if method not in self :
			self.content.append(method)

	def as_cpp_file(self,idt : IndentHandler =None, namespace : T.List[str] = None):
		if idt is None :
			idt = IndentHandler()

		if namespace is None :
			namespace = list()

		ret = (f'#pragma once\n'
			   f'{idt}#include <string>\n'
			   f'{idt}#include <unordered_set>\n'
			   f'\n')
		if len(namespace) :
			ret += f'namespace {"::".join(namespace)} {{\n'
			idt.add_indent_level()
		ret += f'{idt}const std::unordered_set<std::string> RESERVED_METHODS{{\n{idt+1}'
		idt.add_indent_level()
		ret += f",\n{idt}".join([f'"{x.name}"' for x in self.content])
		idt.sub_indent_level()
		ret += f'\n{idt}}};\n'

		if len(namespace) :
			idt.sub_indent_level()
			ret += f'{idt}}}\n'

		return ret