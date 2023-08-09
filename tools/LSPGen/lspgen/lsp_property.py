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
		if len(self.documentation) > 0:
			ret += f"{idt}/**\n"
			for line in self.documentation.split("\n") :
				ret += f"{idt} * {line}\n"
			ret += f"{idt} */\n"

		ret += f"{idt}{self.cpp_type} {self.name};\n"
		return ret







