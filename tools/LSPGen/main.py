
if __name__ == "__main__":
	import argparse

	parser = argparse.ArgumentParser()
	parser.add_argument("input",type=str,help="Meta-model JSON file to process")
	parser.add_argument("--output","-o",type=str,default="out", help="Output root directory")
	parser.add_argument("--force","-f",action="store_true", help="Overwrite output")

	args = parser.parse_args()

	from lspgen.lsp_metareader import LSPMetareader
	from lspgen.lsp_type import LSPType
	from lspgen.indentation import IndentHandler
	import os

	reader = LSPMetareader()

	integer = LSPType("int",optional=None,array=None,nullable=None,include="")
	uinteger = LSPType("unsigned int",optional=None,array=None,nullable=None,include="")
	boolean = LSPType("bool", optional=None, array=None,nullable=None, include="")
	decimal = LSPType("double", optional=None, array=None,nullable=None, include="")



	uri = LSPType("uri",optional=None,array=None,nullable=None,include='"uri.hh"')
	def uri_to_json(t :LSPType, idt : IndentHandler, symbol_name : str ) :
		ret = f'{idt}j["{symbol_name}"] = s.{symbol_name}'
		if t.is_def_optional :
			ret += ".value()"
		ret += ".to_string();\n"
		return ret

	def uri_from_json(t :LSPType, idt : IndentHandler, symbol_name : str ) :
		return f'{idt}s.{symbol_name} = j.at("{symbol_name}").template get<{string.type_name}>();\n'

	uri._to_json_binding = uri_to_json
	uri._from_json_binding = uri_from_json

	json = LSPType("nlohmann::json", optional=None, array=False,nullable=None, include='"nlohmann/json.hpp"')
	def json_from_json(t :LSPType, idt, symbol_name):
		return f'{idt}s.{symbol_name} = j.at("{symbol_name}");\n'

	json._from_json_binding = json_from_json
	DocumentSelector = LSPType("TextDocumentFilter", optional=None, array=True, nullable=None, include=None)
	WorkspaceFullDocumentDiagnosticReport = LSPType("WorkspaceFullDocumentDiagnosticReport", optional=None, array=None, nullable=None)
	string = LSPType("std::string", optional=None, array=None, nullable=None, include="<string>")

	reader.type_override["uinteger"] = uinteger
	reader.type_override["integer"] = integer
	reader.type_override["decimal"] = decimal

	reader.type_override["boolean"] = boolean

	reader.type_override["DocumentUri"] = string
	reader.type_override["URI"] = string
	reader.type_override["GlobPattern"] = string
	reader.type_override["ChangeAnnotationIdentifier"] = string
	reader.type_override["Pattern"] = string

	reader.type_override["json"] = json
	reader.type_override["LSPAny"] = json
	reader.type_override["LSPObject"] = json

	reader.type_override["string"] = string
	reader.type_override["ProgressToken"] = string

	reader.type_override["DocumentSelector"] = json
	reader.type_override["TextDocumentContentChangeEvent"] = json
	reader.type_override["WorkspaceDocumentDiagnosticReport"] = WorkspaceFullDocumentDiagnosticReport


	reader.add_or_resolution(["TextDocumentSyncOptions","TextDocumentSyncKind"],"TextDocumentSyncOptions")

	print(f"Read input file:", os.path.abspath(args.input))
	print(f"Write target   :", os.path.abspath(args.output))
	print(f"Overwrite      :", args.force)
	reader.read(args.input)
	reader.process()
	reader.write_files(args.output,args.force)

