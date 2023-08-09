
if __name__ == "__main__":
	from lspgen.lsp_metareader import LSPMetareader
	from lspgen.lsp_type import LSPType

	reader = LSPMetareader()

	integer = LSPType("int",optional=None,array=None,include="")
	uinteger = LSPType("unsigned int",optional=None,array=None,include="")
	boolean = LSPType("bool", optional=None, array=None, include="")
	decimal = LSPType("double", optional=None, array=None, include="")

	uri = LSPType("uri",optional=None,array=None,include='"uri.hh"')
	json = LSPType("nlhomann::json", optional=None, array=False, include='"nlohmann/json.hpp"')
	string = LSPType("std::string", optional=None, array=None, include="<string>")

	reader.type_override["uinteger"] = uinteger
	reader.type_override["integer"] = integer
	reader.type_override["decimal"] = decimal

	reader.type_override["boolean"] = boolean

	reader.type_override["DocumentUri"] = uri
	reader.type_override["URI"] = uri

	reader.type_override["json"] = json
	reader.type_override["LSPAny"] = json
	reader.type_override["string"] = string

	reader.read("in/metaModel.json")
	reader.process()
	reader.write_files("out")

