from lspgen.indentation import IndentHandler
from lspgen.LSPTyped import LSPTypedBase
from lspgen.lsp_type import LSPType


class LSPProperty(LSPTypedBase):

	def __init__(self,name : str, typename : str, literal_value : str = None ):
		super().__init__(typename)
		self.documentation = ""
		self.name = name

		self.literal_value = literal_value

	def legalize(self):
		self.documentation.strip()

	def as_cpp(self, idt : IndentHandler = None):
		self.legalize()
		if idt is None :
			idt = IndentHandler(base_indent="\t")
		ret = ""
		ret += self._doc.as_cpp(idt)

		if self.literal_value is not None : 
			ret += f"{idt}static constexpr {self.cpp_type} {self.name} = {self.literal_value};\n"
		else : 
			ret += f"{idt}{self.cpp_type} {self.name};\n"
		return ret







