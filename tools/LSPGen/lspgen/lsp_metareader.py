import typing as _T

from lspgen.indentation import IndentHandler
from lspgen.lsp_type import LSPType
from lspgen.lsp_definition import LSPDefinition
from lspgen.lsp_property import LSPProperty

import json


class LSPMetareader:
	def __init__(self):
		self.data = None

		self.type_override : _T.Dict[str,LSPType] = dict()
		self.structures : _T.List[LSPDefinition] = list()


	def read(self, path):
		with open(path, "r") as f:
			self.data = json.load(f)

	def process(self):
		for struct in self.data["structures"] :
			self.add_structure_from_json(struct)

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
		self.structures.append(new_struct)

	def property_from_json(self, elt, master_name = "" ):
		json_type = elt["type"]
		is_combination = json_type["kind"] == "or"
		is_json_litteral = json_type["kind"] == "literal"
		is_string_litteral = json_type["kind"] == "stringLiteral"

		is_combination_optional = False
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
						is_combination_optional = True

			# Select other 'or' Item as the normal type.
			if is_combination_optional:
				for it in elt["type"]["items"]:
					if not (it["kind"] == "base" and it["name"] == "null"):
						json_type = it
						break
			else:
				typename = "json"
		is_array = json_type["kind"] == "array"
		if typename is None:
			try:
				typename = json_type["element"]["name"] if is_array else json_type["name"]
			except KeyError:
				typename = "json"

		new_prop = LSPProperty(prop_name, typename)
		new_prop.type.is_array = is_array
		if "documentation" in elt:
			new_prop.documentation = elt["documentation"]
		if is_combination_optional or "optional" in elt:
			new_prop.type.is_optional = is_combination_optional or elt["optional"]
		if new_prop.type.type_name in self.type_override:
			new_prop.type.update(self.type_override[new_prop.type.type_name])
		return new_prop

	def write(self,path):
		with open(path,"w") as f :
			for d in self.structures :
				f.write(d.as_cpp())

	def write_files(self,root):
		for d in self.structures :
			with open(f"{root}/{d.type.type_name}.hpp","w") as f :
				f.write(d.as_cpp_file(None,["slsp","types"]))
		print(f"Processed {len(self.structures)} structures")










