from lspgen.indentation import IndentHandler
from lspgen.LSPTyped import LSPTypedBase
from lspgen.lsp_type import LSPType


class LSPProperty(LSPTypedBase):

	def __init__(self,name, typename):
		super().__init__(typename)
		self.documentation = ""
		self.name = name

	def legalize(self):
		self.documentation.strip()

	def as_cpp(self, idt : IndentHandler = None):
		self.legalize()
		if idt is None :
			idt = IndentHandler(base_indent="\t")
		ret = ""
		ret += self._doc.as_cpp(idt)

		ret += f"{idt}{self.cpp_type} {self.name};\n"
		return ret







