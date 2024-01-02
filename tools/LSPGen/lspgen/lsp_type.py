import typing as _T
from lspgen.indentation import IndentHandler

class LSPType:
	BASE_OPTIONAL = "std::optional<{} >"
	BASE_ARRAY = "std::vector<{}>"
	def __init__(self, name = "", optional = False, array = False, include = None, nullable = False):
		self.type_name = name
		self.related_include = include if include is not None else (None if name == "" else f'"{name}.hpp"')

		self.is_optional = optional
		self.is_nullable = nullable
		self.is_array = array

		self._to_json_binding : _T.Callable[[LSPType,IndentHandler,str],str] = None
		self._from_json_binding : _T.Callable[[LSPType,IndentHandler,str],str] = None


	def __str__(self):
		ret = "{}"
		if self.is_def_optional:
			ret = ret.format(self.BASE_OPTIONAL)
		if self.is_array:
			ret = ret.format(self.BASE_ARRAY)

		return ret.format(str(self.type_name))

	@property
	def is_def_optional(self):
		return self.is_optional or self.is_nullable

	def from_json(self, idt : IndentHandler, symbol_name : str):
		ret = ""
		add_level = 0
		if self.is_def_optional:
			if self.is_optional:
				ret += f'{idt}if (j.contains("{symbol_name}"))\n'
				add_level += 1
			if self.is_nullable:
				ret += f'{idt + add_level}if (! j["{symbol_name}"].is_null())\n'
				add_level += 1

		if self._from_json_binding is not None:
			ret += self._from_json_binding(self, idt + add_level, symbol_name)
		else:
			ret += self._default_from_json_binding(idt + add_level, symbol_name)

		if self.is_def_optional:
			ret += "\n"

		return ret

	def to_json(self,idt: IndentHandler,symbol_name):
		ret = ""
		add_level = 0
		if self.is_def_optional:
			ret += f'{idt}if (s.{symbol_name})\n' \
				   f'{idt}{{\n'
			add_level += 1

		if self._to_json_binding is not None :
			ret += self._to_json_binding(self,idt + add_level, symbol_name)
		else :
			ret += self._default_to_json_binding(idt + add_level, symbol_name)

		if self.is_def_optional :
			ret += f'{idt}}}\n'
		if self.is_nullable and not self.is_optional:
				ret += f'{idt}else\n' \
					   f'{idt}{{\n' \
				       f'{idt+1}j["{symbol_name}"] = nullptr;\n' \
					   f'{idt}}}\n'

		return ret

	def _default_from_json_binding(self, idt, symbol_name):
		the_type = self.type_name
		if self.is_array :
			the_type = self.BASE_ARRAY.format(the_type)
		return f'{idt}s.{symbol_name} = j.at("{symbol_name}").template get<{the_type}>();\n'

	def _default_to_json_binding(self, idt, symbol_name):
		ret = f'{idt}j["{symbol_name}"] = s.{symbol_name}'
		if self.is_def_optional :
			ret += ".value()"
		ret += ";\n"
		return ret

	def update(self,other : "LSPType"):
		if other.type_name is not None :
			self.type_name = other.type_name
		if other.is_optional is not None :
			self.is_optional = other.is_optional
		if other.is_array is not None :
			self.is_array = other.is_array
		if other.related_include is not None :
			self.related_include = other.related_include
		if other.is_nullable is not None :
			self.is_nullable = other.is_nullable
		if other._to_json_binding is not None :
			self._to_json_binding = other._to_json_binding
		if other._from_json_binding is not None :
			self._from_json_binding = other._from_json_binding
