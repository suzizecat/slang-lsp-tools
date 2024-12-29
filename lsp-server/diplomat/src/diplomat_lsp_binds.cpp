#include "diplomat_lsp.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <algorithm>
#include "types/structs/SetTraceParams.hpp"

#include "slang/diagnostics/AllDiags.h"
#include <slang/ast/symbols/CompilationUnitSymbols.h>
#include <slang/ast/Symbol.h>
#include <slang/ast/Scope.h>

#include "types/structs/InitializeParams.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/ConfigurationParams.hpp"
#include "types/structs/ConfigurationItem.hpp"
#include "types/structs/DidChangeWorkspaceFoldersParams.hpp" 
#include "types/structs/DidOpenTextDocumentParams.hpp" 
#include "types/structs/DidSaveTextDocumentParams.hpp" 
#include "types/structs/RegistrationParams.hpp" 
#include "types/structs/Registration.hpp" 
#include "types/structs/DefinitionParams.hpp" 
#include "types/structs/Location.hpp" 

#include "slang/syntax/SyntaxPrinter.h"
#include "format_DataDeclaration.hpp"
#include "spacing_manager.hpp"
// UNIX only header
#include <wait.h>
#include <fstream>

#include "uri.hh"

#include "hier_visitor.h"

using namespace slsp::types;

namespace fs = std::filesystem;
namespace di = diplomat::index;

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Utility
void trim(std::string& in) {
	ltrim(in);
	rtrim(in);
}


void DiplomatLSP::_h_didChangeWorkspaceFolders(json _)
{
	DidChangeWorkspaceFoldersParams params = _.template get<DidChangeWorkspaceFoldersParams>();
	_remove_workspace_folders(params.event.removed);
	_add_workspace_folders(params.event.added);
}

void DiplomatLSP::_h_didSaveTextDocument(json _)
{
	DidSaveTextDocumentParams param = _ ;
	_read_document(fs::path("/" + uri(param.textDocument.uri).get_path()));
	_compile();
}

void DiplomatLSP::_h_didOpenTextDocument(json _)
{
	DidOpenTextDocumentParams params =  _;
	_save_client_uri(params.textDocument.uri);
}   

json DiplomatLSP::_h_formatting(json _)
{
	DocumentFormattingParams params =  _;
	std::string filepath = "/" + uri(params.textDocument.uri).get_path();
	spdlog::info("Request to format the file {}",filepath);
	slang::SourceManager sm;
	slang::BumpAllocator mem;
	SpacingManager idt(mem,params.options.tabSize,!params.options.insertSpaces);
	auto st = slang::syntax::SyntaxTree::fromFile(filepath,sm).value();
	idt.add_level();
	DataDeclarationSyntaxVisitor fmter(&idt);
	std::shared_ptr<slang::syntax::SyntaxTree> formatted = fmter.transform(st);


	Position end;
	end.line = sm.getLineNumber(st->root().getLastToken().range().end());
	end.character = sm.getColumnNumber(st->root().getLastToken().range().end());

	Position start = {0,0};
	
	json ret_cont = json::array();
	TextEdit ret;
	ret.range = {start,end};
	ret.newText = slang::syntax::SyntaxPrinter::printFile(*formatted);

	ret_cont.push_back(ret);
	spdlog::debug("Formatted output: {}",ret.newText);
	return ret_cont;
}   


void DiplomatLSP::_h_exit(json params)
{
	exit();
}

json DiplomatLSP::_h_initialize(json params)
{
	using namespace std::chrono_literals;
	InitializeParams p = params.template get<InitializeParams>();
	bool got_workspace = false;
	_client_capabilities = p.capabilities;
	if(p.capabilities.workspace && p.capabilities.workspace.value().workspaceFolders.value_or(false))
	{
		if (p.workspaceFolders)
		{
			_add_workspace_folders(p.workspaceFolders.value());
			got_workspace = true;
		}
	}

	if (!got_workspace)
	{
		if (p.rootUri)
		{
			spdlog::info("Add root directory from URI: {}", p.rootUri.value());
			//_root_dirs.push_back(fs::path(uri(p.rootUri.value()).get_path()));
			_settings.workspace_dirs.emplace(fs::path(uri(p.rootUri.value()).get_path()));
		}
		else if(p.rootPath)
		{
			spdlog::info("Add root directory from path: {}", p.rootPath.value());
			//_root_dirs.push_back(fs::path(p.rootPath.value()));
			_settings.workspace_dirs.emplace(fs::path(p.rootPath.value()));
			
		}
		
	}

	if (p.processId && _watch_client_pid)
	{
	
		spdlog::info("Watching client PID {}", p.processId.value());
		_pid_watcher = std::thread(
			[this](int pid) {
				while(kill(pid,0) == 0)
				{
					std::this_thread::sleep_for(1ms);
				}
				spdlog::warn("Client process {} exited, stopping the server.", pid);
				this->_rpc.abort();
			},
			p.processId.value()
		);
	}

	InitializeResult reply;
	reply.capabilities = capabilities;
	reply.serverInfo = InitializeResult_serverInfo{"Diplomat-LSP","0.0.1"};

	set_initialized(true) ;
	return reply;
}

void DiplomatLSP::_h_initialized(json params)
{
	spdlog::info("Client initialization complete.");

	slsp::types::RegistrationParams p;
	if (_client_capabilities.workspace 
	&&  _client_capabilities.workspace.value().didChangeConfiguration
	&& _client_capabilities.workspace.value().didChangeConfiguration.value().dynamicRegistration
	&& _client_capabilities.workspace.value().didChangeConfiguration.value().dynamicRegistration.value())
	{
		slsp::types::Registration didChangeRegistration;
		didChangeRegistration.id = uuids::to_string(_uuid());
		didChangeRegistration.method = "workspace/didChangeConfiguration";
		p.registrations.push_back(didChangeRegistration);
	}

	// send_notification("client/registerCapability",p);
	

	slsp::types::ConfigurationItem conf_path;
	conf_path.section = "diplomatServer.server.configurationPath";

	slsp::types::ConfigurationItem index_ext;
	index_ext.section = "diplomatServer.index.validExtensions";

	slsp::types::ConfigurationParams conf_request;
	conf_request.items.push_back(conf_path);
	conf_request.items.push_back(index_ext);
	
	if (_client_capabilities.workspace
		&& _client_capabilities.workspace.value_or(slsp::types::WorkspaceClientCapabilities{}).configuration.value_or(false)
		&& _client_capabilities.workspace.value().configuration.value())
		send_request("workspace/configuration", LSP_MEMBER_BIND(DiplomatLSP, _h_get_configuration_on_init), conf_request);

}

void DiplomatLSP::_h_setTrace(json params)
{
	SetTraceParams p = params;
	spdlog::info("Set trace through params {}",params.dump());
	log(MessageType::MessageType_Log,"Setting trace level to " + params.at("value").template get<std::string>());
	set_trace_level(p.value);
}

json DiplomatLSP::_h_shutdown(json params)
{
	shutdown();
	//exit();
	return json();
}


json DiplomatLSP::_h_gotoDefinition(json _)
{
	if (!_assert_index())
		return {};
	
	slsp::types::DefinitionParams params = _;
	slsp::types::Location result;

	const di::IndexLocation lu_location = _lsp_to_index_location(params);
	const di::IndexSymbol* lu_symb = _index->get_symbol_by_position(lu_location);

	if(lu_symb)
	{

		log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookuped up definition for {}", lu_symb->get_name()));
		// If Symbol has been looked up, it has a source, hence no check.
		return _index_range_to_lsp(lu_symb->get_source().value());
	} 
	else
	{
		log(slsp::types::MessageType::MessageType_Log, "Unable to get a symbol from position");
		return {};
	}
	
	
	// auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1 , params.position.character +1);
	// if (syntax)
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}", source_path.generic_string(), params.position.line, params.position.character));
	// 	slang::SourceRange return_range = _index->get_definition(syntax->range());
	// 	spdlog::info("Found definition.");

	// 	return _slang_to_lsp_location(return_range);
	// }
	// else
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
	// 	spdlog::info("Definition not found.");
	// 	return {};
	// }
}


json DiplomatLSP::_h_references(json _)
{


	if (!_assert_index())
		return {};
	
	slsp::types::DefinitionParams params = _;

	const di::IndexLocation lu_location = _lsp_to_index_location(params);
	const di::IndexSymbol* lu_symb = _index->get_symbol_by_position(lu_location);

	if(lu_symb)
	{
		std::vector<Location> result;
		for(const auto& range : lu_symb->get_references())
		{
			result.push_back(_index_range_to_lsp(range));
		}

		return result;
	}

	return {};


	// if (!_assert_index())
	// 	return {};
	// slsp::types::DefinitionParams params = _;
	// slsp::types::Location result;

	// fs::path source_path = fs::canonical(fs::path("/" + uri(params.textDocument.uri).get_path()));

	// auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1, params.position.character+1);
	// if (syntax)
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
	// 	std::vector<slang::SourceRange> return_range = _index->get_references(CONST_TOKNODE_SR(*syntax));
	// 	spdlog::info("Found references.");

	// 	std::vector<Location> result;
	// 	for(const auto& range : return_range)
	// 		result.push_back(_slang_to_lsp_location(range));
		
	// 	return result;
	// }
	// else
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
	// 	spdlog::info("References not found.");
	// 	return {};
	// }
}

json DiplomatLSP::_h_rename(json _)
{

		_assert_index(true);
		
		slsp::types::RenameParams params = _;
		slsp::types::WorkspaceEdit result;

		const di::IndexLocation lu_location = _lsp_to_index_location(params);
		const di::IndexSymbol* lu_symb = _index->get_symbol_by_position(lu_location);

		if(lu_symb)
		{

			std::size_t curr_name_len = lu_symb->get_name().size();
			std::unordered_map<std::string,std::vector<slsp::types::TextEdit>> edits;

			for(const auto& range : lu_symb->get_references())
			{
				slsp::types::Location edit_location = _index_range_to_lsp(range);
				
				if (! edits.contains(edit_location.uri))
					edits[edit_location.uri] = {};

				edits.at(edit_location.uri).push_back(
				slsp::types::TextEdit{
					edit_location.range,
					fmt::format("{:{}s}",params.newName,curr_name_len)
				}
			);
				
			}
			result.changes = edits;
			return result;
		}

		throw slsp::lsp_request_failed_exception("Selected area did not returned a significant symbol");
		return {}; // To avoid missing return


	
	// _assert_index(true);
	// slsp::types::RenameParams params = _;
	// slsp::types::WorkspaceEdit result;

	// fs::path source_path = fs::canonical(fs::path("/" + uri(params.textDocument.uri).get_path()));

	// auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1, params.position.character+1);
	// if (syntax)
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}", source_path.generic_string(), params.position.line, params.position.character));

	// 	slang::SourceRange origin_sr = CONST_TOKNODE_SR(*syntax);
	// 	int origin_len = origin_sr.end() - origin_sr.start();

	// 	std::vector<slang::SourceRange> return_range = _index->get_references(origin_sr);
	// 	spdlog::info("Perform rename.");

	// 	std::unordered_map<std::string,std::vector<slsp::types::TextEdit>> edits;

	// 	for(const auto& range : return_range)
	// 	{
	// 		slsp::types::Location edit_change = _slang_to_lsp_location(range);
	// 		if (! edits.contains(edit_change.uri))
	// 			edits[edit_change.uri] = {};

	// 		edits.at(edit_change.uri).push_back(
	// 			slsp::types::TextEdit{
	// 				edit_change.range,
	// 				fmt::format("{:{}s}",params.newName,origin_len)
	// 			}
	// 		);
	// 	}
	// 	result.changes = edits;
	// 	return result;
	// }
	// else
	// {
	// 	log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
	// 	spdlog::info("References not found.");
	// 	throw slsp::lsp_request_failed_exception("Selected area did not returned a significant syntax node.");
	// }
}



// void DiplomatLSP::_h_save_config(json params)
// {
// 	spdlog::info("Write configuration to {}",fs::canonical(_settings_path).generic_string());
// 	log(slsp::types::MessageType::MessageType_Info,fmt::format("Write configuration to {}",_settings_path.generic_string()));
// 	std::ofstream out(_settings_path);
// 	json j = _settings;
// 	out << j.dump(4);
// }

/**
 * @brief Pushing configuration, from client to server.
 * 
 * @param configuration, from slsp::DiplomatLSPWorkspaceSettings
 */
void DiplomatLSP::_h_push_config(json params)
{
	spdlog::info("Received configuration from client");
	log(slsp::types::MessageType::MessageType_Info,"Received configuration from client");
	spdlog::debug("Config is {}",params.dump(1));
	_settings = params[0];

	show_message(slsp::types::MessageType::MessageType_Info,"Configuration successfully loaded by the server.");
	_compile();

}


/**
 * @brief Pull configuration from the server to the client 
 * 
 * @param params, unused.
 */
json DiplomatLSP::_h_pull_config(json params)
{
	spdlog::info("Send configuration to the client.");
	log(slsp::types::MessageType::MessageType_Info,"Configuration pulled from server");
	json j = _settings;
	return j;
}

void DiplomatLSP::_h_set_top_module(json _)
{
	_settings.top_level = _[0].at("top").template get<std::string>();
}

json DiplomatLSP::_h_get_modules(json params)
{
	_read_workspace_modules();
	json ret = json::array();

	for (const auto& [name, path] : _module_to_file)
	{
		ret.push_back({ {"name", name},{"file", path.generic_string()} });
	}
	return ret;
}


json DiplomatLSP::_h_get_module_bbox(json _)
{


	json params = _[0];
	const std::string target_file = params.at("file").template get<std::string>();
	spdlog::info("Return information for file {}",target_file );
	fs::path lookup_path = fs::canonical(target_file);
	
	ModuleBlackBox* bb = _blackboxes.at(lookup_path).get();
	return *bb;
}

void DiplomatLSP::_h_set_module_top(json params)
{
	fs::path p;
	for (const json& record : params.at(1))
	{
		p = fs::canonical(record["path"].template get<std::string>());
	}

	spdlog::info("Set top file {}", p.generic_string());
	ModuleBlackBox* bb = _blackboxes.at(p).get();
	_settings.top_level = bb->module_name;

	_compute_project_tree();
	_compile();
}

void DiplomatLSP::_h_ignore(json params)
{
	for (const json& record : params.at(1))
	{
		fs::path p = fs::canonical(record["path"].template get<std::string>());
		spdlog::info("Ignore path {}", p.generic_string());
		_settings.excluded_paths.insert(p);
		_project_file_tree_valid = false;
	}
}

/**
 * @brief Handle to add an include directory.
 * This is intended to be used as a command in VSCode for right click action in the 
 * filesystem view.
 * 
 * @param params 
 */
void DiplomatLSP::_h_add_to_include(json params)
{
	for (const json& record : params.at(1))
	{
		fs::path p = fs::canonical(record["path"].template get<std::string>());
		spdlog::info("Add user include path {}", p.generic_string());
		_settings.includes.user.push_back(p.generic_string());
	}
}

void DiplomatLSP::_h_force_clear_index(json _)
{
	_project_file_tree_valid = false;
	_compilation.reset();
	_broken_index_emitted = true;
	_read_workspace_modules();
	_compile();
}

/**
 * @brief Resolves design hierarchical paths and return the location of the definition
 * of the targeted symbols
 * 
 * @param params JSON structure equivalent to a list of hierarchical paths
 * @return json association initial path => Location. Return null on unresolved paths.
 */
json DiplomatLSP::_h_resolve_hier_path(json params)
{
	// Params will be a list of hier paths to resolve.
	json ret;
	if(! _assert_index())
		return ret;
	
	const auto& design_root = _compilation->getRoot();
	for (const std::string& path: params)
	{
		spdlog::debug("Resolve path {}",path);

		di::IndexSymbol* lu_result = _index->get_root_scope()->resolve_symbol(path);

		if(! lu_result)
		{
			spdlog::debug("Resolution yielded no result");
			ret[path] = nullptr;
		}
		else
		{
			ret[path] = _index_range_to_lsp(lu_result->get_source().value());
		}
	}

	return ret;
}

/**
 * @brief Return a JSON view of the design hierarchy
 * 
 * @param _ 
 * @return json 
 */
json DiplomatLSP::_h_get_design_hierarchy(json _)
{
	json ret;

	if(! _assert_index())
	{
		return ret;
	}
	const slang::ast::RootSymbol& design_root = _compilation->getRoot();
	HierVisitor hier_visitor(false,&_doc_path_to_client_uri);

	design_root.visit(hier_visitor);

	return hier_visitor.get_hierarchy();

}

void DiplomatLSP::_h_get_configuration(json &clientinfo)
{
	//TODO Cleanup
	// _settings_path = fs::path(clientinfo[0]);
	
	_accepted_extensions.clear();
	for(const std::string& ext : clientinfo[1])
	{
		_accepted_extensions.emplace(ext);
		spdlog::debug("Add accepted extension {}",ext);
	}   


	// if(fs::exists(_settings_path))
	// {
	// 	spdlog::info("Read configuration file {}",_settings_path.generic_string());
	// 	std::ifstream conf_file(_settings_path);
	// 	json conf = json::parse(conf_file) ;
	// 	_settings = conf.template get<slsp::DiplomatLSPWorkspaceSettings>();
	// }
}

void DiplomatLSP::_h_get_configuration_on_init(json &clientinfo)
{
	_h_get_configuration(clientinfo);
	//_compile();
}

void DiplomatLSP::_h_update_configuration(json &params)
{
	spdlog::info("Update configuration received {}",params.dump(1));
	slsp::types::ConfigurationItem conf_path;
	conf_path.section = "diplomatServer.server.configurationPath";

	slsp::types::ConfigurationParams conf_request;
	conf_request.items.push_back(conf_path);
	send_request("workspace/configuration",LSP_MEMBER_BIND(DiplomatLSP,_h_get_configuration),conf_request);
}

json DiplomatLSP::_h_list_symbols(json& params)
{
	std::string path = params.at(0);
	std::map<std::string,std::vector<slsp::types::Range>> ret;
	
	const di::IndexFile* lu_file = _index->get_file(path);

	if(! lu_file)
		return ret;

	const std::map<di::IndexLocation, di::ReferenceRecord>& refs = lu_file->get_references();

	for(const auto& symbol : lu_file->get_symbols())
	{
		ret[symbol->get_name()] = {};
	}

	for(const auto& [loc, refrec] : refs)
	{
		di::IndexRange ref_range(loc,refrec.key->get_name().size());
		ret.at(refrec.key->get_name()).push_back(_index_range_to_lsp(ref_range).range);
	}


	
	// auto* compilation = get_compilation();
	// const slang::ast::Symbol* lookup_result = compilation->getRoot().lookupName(path);
	// const slang::ast::Scope* scope;

	// if( lookup_result->kind == slang::ast::SymbolKind::Instance )
	// 	lookup_result = &(lookup_result->as<slang::ast::InstanceSymbol>().body);

	// if(! lookup_result->isScope())
	// 	scope = lookup_result->getParentScope();
	// else
	// 	scope = &(lookup_result->as<slang::ast::Scope>());

	// spdlog::info("Request design list of symbol for path {}", path);
	// const slang::ast::InstanceBodySymbol* target_instance_body = scope->getContainingInstance();

	// if (!target_instance_body)
	// 	throw slsp::lsp_request_failed_exception("Failed to lookup the required instance");

	// std::filesystem::path file_for_index_lookup = _module_to_file.at(std::string(target_instance_body->name));

	// std::unordered_map<std::string,std::vector<slsp::types::Range>> ret;
	// for (const slang::syntax::ConstTokenOrSyntax& tok : _index->get_symbols_tok_from_file(file_for_index_lookup))
	// {
	// 	std::string name = std::string(tok.isNode() ? tok.node()->getFirstToken().valueText() : tok.token().valueText());
	// 	trim(name);

	// 	if (!name.empty())
	// 	{
	// 		slsp::types::Range tok_range = _slang_to_lsp_location(tok.range()).range;

	// 		if (!ret.contains(name))
	// 			ret[name] = { tok_range };
	// 		else
	// 			ret[name].push_back(tok_range);
			
			
	// 		spdlog::debug("    Return {}", name);
	// 	}
	// }

	return ret;
}