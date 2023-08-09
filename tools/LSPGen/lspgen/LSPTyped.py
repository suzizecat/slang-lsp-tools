import typing
from lspgen.lsp_type import LSPType


class LSPTypedBase:
	def __init__(self, name = ""):
		self.type : typing.Union[str,LSPType,None] = LSPType(name)

	@property
	def cpp_type(self):
		return str(self.type)



