class LSPType:
	BASE_OPTIONAL = "std::optional<{} >"
	BASE_ARRAY = "std::vector<{}>"
	def __init__(self, name = "", optional = False, array = False, include = None, nullable = False):
		self.type_name = name
		self.related_include = include if include is not None else (None if name == "" else f'"{name}.hpp"')

		self.is_optional = optional
		self.is_nullable = nullable
		self.is_array = array


	def __str__(self):
		ret = "{}"
		if self.is_optional:
			ret = ret.format(self.BASE_OPTIONAL)
		if self.is_array:
			ret = ret.format(self.BASE_ARRAY)

		return ret.format(str(self.type_name))

	@property
	def is_def_optional(self):
		return self.is_optional or self.is_nullable

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