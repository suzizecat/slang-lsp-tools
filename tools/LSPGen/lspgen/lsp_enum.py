import typing as _T
from lspgen.indentation import IndentHandler
from lspgen.LSPTyped import LSPTypedBase, LSPBaseDocumented
from lspgen.lsp_type import LSPType
from lspgen.lsp_property import LSPProperty


class LSPEnumValue(LSPBaseDocumented):
	def __init__(self, name : str, value = None, documentation = None):
		super().__init__(documentation)
		self.name = name
		self.value = value
		self.json_value = value

	@property
	def cpp_jsonvalue(self) -> str:
		if self.json_value is None :
			return "nullptr"
		else :
			return f'"{self.json_value}"'



class LSPEnum(LSPTypedBase):
	AUTO_TYPES = ["int","unsigned int","double"]
	def __init__(self,name,base_type,documentation = None):
		super().__init__(name,documentation)
		self.value_type = LSPType(base_type)

		self.enum_values : _T.List[LSPEnumValue] = list()

	@property
	def require_json_bindings(self):
		return self.value_type.type_name not in self.AUTO_TYPES

	def finalize(self):
		if self.value_type.type_name not in self.AUTO_TYPES :
			invalid_handler = LSPEnumValue(f"_{self.type.type_name}_Invalid",-1,"Automatically added by generator as invalid handler")
			invalid_handler.json_value = None
			self.enum_values.insert(0,invalid_handler)

	def as_cpp(self,idt : IndentHandler):
		if idt is None:
			idt = IndentHandler()
		ret = self._doc.as_cpp(idt)
		ret += f"{idt}typedef enum {self.type.type_name} {{\n"
		idt.add_indent_level()

		started = False
		for value in self.enum_values:
			if started :
				ret += ",\n\n"
			else :
				started = True

			ret += value._doc.as_cpp(idt)
			ret += f"{idt}{self.type.type_name}_{value.name}"
			if self.value_type.type_name in ["int", "unsigned int"] :
				if value.value is not None :
					ret += f" = {value.value}"

		ret += f"\n"
		idt.sub_indent_level()

		ret += f"{idt}}} {self.type.type_name};\n"

		return ret
	
	def json_bind_prototypes(self,idt : IndentHandler = None):
		if idt is None:
			idt = IndentHandler()
		ret = (f"{idt}void to_json(nlohmann::json& j, const {self.type.type_name}& s);\n"
				f"{idt}void from_json(const nlohmann::json& j, {self.type.type_name}& s);\n")
		return ret

	def as_cpp_to_json(self,idt : IndentHandler):
		if idt is None:
			idt = IndentHandler()

		if not self.require_json_bindings :
			return ""

		ret = f"\n{idt}NLOHMANN_JSON_SERIALIZE_ENUM({self.type.type_name},{{\n"
		idt.add_indent_level()

		for value in self.enum_values :
			ret += f"{idt}{{{self.type.type_name}_{value.name},{value.cpp_jsonvalue}}},\n"

		idt.sub_indent_level()
		ret += f"{idt}}})\n"

		return ret

	def as_cpp_file(self,idt:IndentHandler = None, namespace : _T.List[str] = None ):
		if namespace is None :
			namespace = list()
		if idt is None:
			idt = IndentHandler()

		ret = f"{idt}#pragma once\n\n"
		# for inc in self.get_include_list() :
		if self.require_json_bindings :
			ret += f'{idt}#include "nlohmann/json.hpp"\n\n'

		for ns in namespace :
			ret += f"{idt}namespace {ns} {{\n"
			idt.add_indent_level()

		ret += self.as_cpp(idt)
		ret += "\n"

		if self.require_json_bindings :
			# ret += self.json_bind_prototypes(idt)
			ret += self.as_cpp_to_json(idt)
		ret += "\n"

		for ns in namespace :
			idt.sub_indent_level()
			ret += f"{idt}}}; // namespace {ns} \n"

		return ret
