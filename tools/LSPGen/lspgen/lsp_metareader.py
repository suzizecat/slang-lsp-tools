import typing as _T

from lspgen.indentation import IndentHandler
from lspgen.lsp_type import LSPType
from lspgen.lsp_methods import LSPMethod, LSPMethodCollection
from lspgen.lsp_definition import LSPDefinition
from lspgen.lsp_property import LSPProperty
from lspgen.lsp_enum import LSPEnumValue, LSPEnum

import json

import os


class LSPMetareader:
	def __init__(self):
		self.data = None

		self.type_override : _T.Dict[str,LSPType] = dict()
		self.structures : _T.Dict[str,LSPDefinition] = dict()
		self.enumerations : _T.List[LSPEnum] = list()
		self.methods : LSPMethodCollection = LSPMethodCollection()

		self.or_resolution : _T.Dict[str,str] = dict()


	def read(self, path):
		with open(path, "r",encoding="utf-8") as f:
			text = f.read()
			text = text.encode("ascii","ignore")
			self.data = json.loads(text)

	def process(self):
		for enum in self.data["enumerations"] :
			self.add_enum_from_json(enum)
		for struct in self.data["structures"] :
			self.add_structure_from_json(struct)
		for method in self.data["requests"] :
			self.add_method_from_json(method)
		for method in self.data["notifications"] :
			self.add_method_from_json(method)

		self.resolve_structures_extends()

	def resolve_structures_extends(self):
		for struct in self.structures.values() :
			for ext in struct.extend_list :
				if ext.type_name in self.structures :
					struct.extend_definitions.append(self.structures[ext.type_name])

	def add_method_from_json(self,struct):
		method = LSPMethod(struct["method"])
		self.methods.append(method)

	def add_structure_from_json(self, struct, override_name : str = None):
		new_struct = LSPDefinition(struct["name"] if override_name is None else override_name)
		if "documentation" in struct:
			new_struct.documentation = struct["documentation"]
		for src in ["extends", "mixins"]:
			if src in struct:
				for elt in struct[src]:
					ext_type = LSPType(elt["name"])
					if ext_type.type_name in self.type_override:
						ext_type.update(self.type_override[ext_type.type_name])
					new_struct.extend_list.append(ext_type)
		for elt in struct["properties"]:
			new_prop = self.property_from_json(elt, master_name=new_struct.type.type_name)
			new_struct.properties.append(new_prop)
		self.structures[new_struct.type.type_name] = new_struct

	@staticmethod
	def make_or_resolution_key(or_values):
		return "".join(sorted(or_values))
	def add_or_resolution(self,or_values,resolution : str):
		key = self.make_or_resolution_key(or_values)
		self.or_resolution[key] = resolution

	def add_enum_from_json(self,enum):
		enum_name = enum["name"]
		enum_type = enum["type"]["name"]
		enum_doc = None if "documentation" not in enum else enum["documentation"]

		enumeration = LSPEnum(enum_name,enum_type,enum_doc)
		if enumeration.value_type.type_name in self.type_override :
			enumeration.value_type.update(self.type_override[enumeration.value_type.type_name])

		# Required to rewrite the include path.
		if enumeration.type.type_name not in self.type_override :
			bind = LSPType(enumeration.type.type_name,optional=None,array=None,nullable=None,include=f'"../enums/{enumeration.type.type_name}.hpp"')
			self.type_override[enumeration.type.type_name] = bind

		for value in enum["values"] :
			enumeration.enum_values.append(LSPEnumValue(value["name"],value["value"],value["documentation"] if "documentation" in value else None))

		enumeration.finalize()
		self.enumerations.append(enumeration)


	def property_from_json(self, elt, master_name = "" ):
		json_type = elt["type"]
		is_combination = json_type["kind"] == "or"
		is_json_litteral = json_type["kind"] == "literal"
		is_string_litteral = json_type["kind"] == "stringLiteral"

		is_combination_nullable = False
		is_combination_array = False

		prop_name = elt["name"]
		typename = None
		if is_json_litteral:
			typename = f"{master_name}_{prop_name}"
			self.add_structure_from_json(json_type["value"],typename)

		if is_string_litteral:
			typename = "string"

		if is_combination:
			if len(json_type["items"]) == 2:
				for it in json_type["items"]:
					if it["kind"] == "base" and it["name"] == "null":
						is_combination_nullable = True

			# Select other 'or' Item as the normal type.
			if is_combination_nullable:
				for it in elt["type"]["items"]:
					if not (it["kind"] == "base" and it["name"] == "null"):
						json_type = it
						break
			else:
				try :
					key = self.make_or_resolution_key([it["name"] for it in json_type["items"]])
					if key in self.or_resolution :
						typename = self.or_resolution[key]
					else :
						typename = "json"
				except KeyError :
					typename = "json"

		is_array = json_type["kind"] == "array"
		if typename is None:
			try:
				typename = json_type["element"]["name"] if is_array else json_type["name"]
			except KeyError:
				typename = "json"

		if typename == master_name :
			typename = "json"
		new_prop = LSPProperty(prop_name, typename)
		new_prop.type.is_array = is_array
		new_prop.type.is_nullable = is_combination_nullable
		if "documentation" in elt:
			new_prop.documentation = elt["documentation"]
		if  "optional" in elt:
			new_prop.type.is_optional =  elt["optional"]
		if new_prop.type.type_name in self.type_override:
			new_prop.type.update(self.type_override[new_prop.type.type_name])
		return new_prop

	def write(self,path):
		with open(path,"w") as f :
			for d in self.structures.values() :
				f.write(d.as_cpp())

	def write_files(self,root,force_write = False):
		struct_path = f"{root}/structs"
		methods_path = f"{root}/methods"
		enum_path = f"{root}/enums"
		sources_path = f"{root}/src"
		os.makedirs(struct_path,exist_ok=force_write)
		os.makedirs(methods_path,exist_ok=force_write)
		os.makedirs(enum_path, exist_ok=force_write)
		os.makedirs(sources_path, exist_ok=force_write)

		include_list = []
		generated_list = []
		for d in self.structures.values() :
			with open(f"{struct_path}/{d.type.type_name}.hpp","w") as f :
				generated_list.append(f.name)
				include_list.append(f"../structs/{d.type.type_name}.hpp")
				f.write(d.as_cpp_file(None,["slsp","types"]))
		print(f"Processed {len(self.structures)} structures")

		with open(f"{sources_path}/structs_json_bindings.cpp","w") as f :
			includes = "\n".join([f'#include "{x}"' for x in include_list])
			idt = IndentHandler()
			f.write(includes)
			f.write(f"\n\n{idt}namespace slsp::types {{\n")
			idt.add_indent_level()
			for s in self.structures.values():
				f.write(s.json_binding(idt))
			idt.sub_indent_level()
			f.write("}")

		enum_list : _T.List[LSPEnum]= list()
		for e in self.enumerations :
			with open(f"{enum_path}/{e.type.type_name}.hpp","w") as f :
				generated_list.append(f.name)
				if e.require_json_bindings :
					enum_list.append(e)
				f.write(e.as_cpp_file(None,["slsp","types"]))
		
		
		with open(f"{sources_path}/enums_json_bindings.cpp","w") as f :
			include_list = [f"../enums/{e.type.type_name}.hpp" for e in enum_list]
			includes = "\n".join([f'#include "{x}"' for x in include_list])
			idt = IndentHandler()
			f.write(includes)
			f.write(f"\n\n{idt}namespace slsp::types {{\n")
			idt.add_indent_level()
			for e in enum_list:
				f.write(e.as_cpp_to_json(idt))
			idt.sub_indent_level()
			f.write("}")


		print(f"Processed {len(self.enumerations)} enums")



		with open(f"{methods_path}/lsp_reserved_methods.hpp","w") as f :
			generated_list.append(f.name)
			f.write(self.methods.as_cpp_file(None,["slsp","types"]))
		print(f"Processed {len(self.methods.content)} methods")

		with open(f"{root}/generated_headers.cmake","w") as f:
			generated_list = sorted([x.removeprefix(root + "/") for x in generated_list])
			content = f"set(SVLS_GEN_HEADERS " + '\n'.join(generated_list) + ")"
			f.write(content)











