from lspgen.indentation import IndentHandler

class LSPDocstring:
	def __init__(self,text = None):
		self.text = text

	def as_cpp(self,idt : IndentHandler = None) -> str:
		if idt is None :
			idt = IndentHandler()
		ret = ""
		if self.text is not None and len(self.text) > 0:
			ret += f"{idt}/**\n"
			for line in self.text.split("\n") :
				line.strip()
				ret += f"{idt} * {line}\n"
			ret += f"{idt} */\n"

		return ret
