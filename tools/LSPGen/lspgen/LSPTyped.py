import typing
from lspgen.lsp_docstring import LSPDocstring
from lspgen.lsp_type import LSPType


class LSPBaseDocumented:
	def __init__(self, doc = None):
		self._doc = LSPDocstring(doc)

	@property
	def documentation(self):
		return self._doc.text

	@documentation.setter
	def documentation(self,val):
		self._doc.text = val


class LSPTypedBase(LSPBaseDocumented):
	def __init__(self, name = "", documentation = None):
		super().__init__(documentation)
		self.type : typing.Union[str,LSPType,None] = LSPType(name)


	@property
	def cpp_type(self):
		return str(self.type)



