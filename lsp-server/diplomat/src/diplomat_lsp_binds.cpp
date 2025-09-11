#include "diplomat_lsp.hpp"
#include "lsp_errors.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <algorithm>
#include "types/enums/DiagnosticTag.hpp"
#include "types/enums/LSPErrorCodes.hpp"
#include "types/enums/MessageType.hpp"
#include "types/structs/HDLModule.hpp"
#include "types/structs/SetTraceParams.hpp"

#include "slang/diagnostics/AllDiags.h"
#include <filesystem>
#include <fmt/format.h>
#include <optional>
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
#include <string>
#include <sys/wait.h>
#include <fstream>
#include <vector>

#include "uri.hh"

#include "hier_visitor.h"
#include "visitor_module_bb.hpp"

#include "signal.h"

#ifndef DIPLOMAT_VERSION_STRING
#define DIPLOMADIPLOMAT_VERSION_STRING "custom-build"
#endif

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

void DiplomatLSP::_h_didSaveTextDocument(DidSaveTextDocumentParams param)
{
	_cache.process_file(uri(param.textDocument.uri));
	_compile();
}

void DiplomatLSP::_h_didOpenTextDocument(json _)
{
	DidOpenTextDocumentParams params =  _;

	_save_client_uri(params.textDocument.uri);
}

void DiplomatLSP::_h_didCloseTextDocument(DidCloseTextDocumentParams _)
{
	/* Actually nothing to do, only bind to avoid errors*/
}

json DiplomatLSP::_h_completion(CompletionParams params)
{
	CompletionList result;
	result.isIncomplete = false;
	_assert_index(true);

	di::IndexLocation trigger_location = _lsp_to_index_location(params);
	di::IndexScope* trigger_scope = _index->get_scope_by_position(trigger_location);

	if(trigger_scope)
	{
		//spdlog::info("Required completion on valid scope");
		for(const di::IndexSymbol* symb : trigger_scope->get_visible_symbols() )
		{
			if(symb->get_source_location().value_or(trigger_location) > trigger_location)
				continue;

			CompletionItem record;
			record.label = symb->get_name();
			record.kind = CompletionItemKind::CompletionItemKind_Variable;
			result.items.push_back(record);
		}
	}
	else
	{
		// Propose symbols from the whole file.

		//spdlog::info("Required completion on full file");
		di::IndexFile* trigger_file = _index->get_file(trigger_location.file);
		// This file is not known by the indexer.
		if(! trigger_file)
			return result; 
		for(const auto& symb : trigger_file->get_symbols() )
		{
			if(symb->get_source_location().value_or(trigger_location) > trigger_location)
				continue;

			CompletionItem record;
			record.label = symb->get_name();
			record.kind = CompletionItemKind::CompletionItemKind_Variable;
			result.items.push_back(record);
		}
	}

	spdlog::info("    Returned {} propositions",result.items.size());

	return result;
}

json DiplomatLSP::_h_formatting(DocumentFormattingParams params)
{
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

json DiplomatLSP::_h_initialize(InitializeParams params)
{
	using namespace std::chrono_literals;
	bool got_workspace = false;
	_client_capabilities = params.capabilities;
	if(params.capabilities.workspace && params.capabilities.workspace.value().workspaceFolders.value_or(false))
	{
		if (params.workspaceFolders)
		{
			_add_workspace_folders(params.workspaceFolders.value());
			got_workspace = true;
		}
	}

	if (!got_workspace)
	{
		if (params.rootUri)
		{
			spdlog::info("Add root directory from URI: {}", params.rootUri.value());
			//_root_dirs.push_back(fs::path(uri(params.rootUri.value()).get_path()));
			_settings.workspace_dirs.emplace(fs::path(uri(params.rootUri.value()).get_path()));
		}
		else if(params.rootPath)
		{
			spdlog::info("Add root directory from path: {}", params.rootPath.value());
			//_root_dirs.push_back(fs::path(params.rootPath.value()));
			_settings.workspace_dirs.emplace(fs::path(params.rootPath.value()));
			
		}
		
	}

	if (params.processId && _watch_client_pid)
	{
	
		spdlog::info("Watching client PID {}", params.processId.value());
		_pid_watcher = std::jthread(
			[this](std::stop_token stop_token, int pid) {
				while(! stop_token.stop_requested() && kill(pid,0) == 0)
				{
					std::this_thread::sleep_for(100ms);
				}
				spdlog::warn("Client process {} exited, stopping the server.", pid);
				this->_rpc.close();
			},
			params.processId.value()
		);
	}

	InitializeResult reply;
	reply.capabilities = capabilities;
	reply.serverInfo = InitializeResult_serverInfo{"Diplomat-LSP",DIPLOMAT_VERSION_STRING};

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


json DiplomatLSP::_h_gotoDefinition(slsp::types::DefinitionParams params)
{
	if (!_assert_index())
		return {};
	
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
 * @brief This command setup the active project, it will provide the whole
 * list of files, the associated top level module and so on.
 * 
 * Calling this method is equivalement to setting the active project and updating the source list.
 * 
 * @param json structure matching a list with only a single element in it, being 
 * the image of diplomat-vscode/exchange_types.ts:DiplomatProject
 */
void DiplomatLSP::_h_set_project(DiplomatProject params)
{
	spdlog::debug("Set Project requested : {}",json(params).dump(1));
	_clear_project_tree();

	for(const std::string& file_uri : params.sourceList)
	{	
		_cache.process_file(uri(file_uri),true);
	}

	// We assume that the project file tree is valid.
	_project_file_tree_valid = true;

	_included_folders.clear();

	for(const std::string& incpath : params.includeDirs)
		_included_folders.push_back(_cache.standardize_path(incpath).generic_string());

	if (params.topLevel && params.topLevel->moduleName)
		set_top_level(params.topLevel->moduleName.value());
	else
		_compile();
}


/**
 * @brief Pushing configuration, from client to server.
 * 
 * @param configuration, from slsp::DiplomatLSPWorkspaceSettings
 */
void DiplomatLSP::_h_push_config(slsp::DiplomatLSPWorkspaceSettings params)
{
	spdlog::info("Received configuration from client");
	log(slsp::types::MessageType::MessageType_Info,"Received configuration from client");
	spdlog::debug("Config is {}",json(params).dump(1));
	_settings = params;

	show_message(slsp::types::MessageType::MessageType_Info,"Configuration successfully loaded by the server.");
	_compile();

}


/**
 * @brief Pull configuration from the server to the client 
 * 
 * @param params, unused.
 */
json DiplomatLSP::_h_pull_config(json _)
{
	spdlog::info("Send configuration to the client.");
	log(slsp::types::MessageType::MessageType_Info,"Configuration pulled from server");
	json j = _settings;
	return j;
}

/**
 * @brief Return the list of blackboxes from the file URI provided.
 * 
 * @param params One single URI
 * @return json Array of BB (may be empty)
 */
const std::vector< const ModuleBlackBox*>  DiplomatLSP::_h_get_file_bb(std::string params)
{
	spdlog::debug("Get BBOX on {}",params);
	uri target_file =  uri(params);
	fs::path target_path = "/" + target_file.get_path();
	spdlog::debug("Target is {}",target_file.get_path());
	const std::vector<const ModuleBlackBox*> * bb_list = _cache.get_bb_by_file(target_path);
	if(bb_list == nullptr)
	{
		_cache.process_file(target_path);
		bb_list = _cache.get_bb_by_file(target_path);
	}

	if(bb_list == nullptr)
		throw slsp::lsp_request_failed_exception("File processing failed");

	return *bb_list;

}

std::vector<slsp::types::HDLModule> DiplomatLSP::_h_get_modules(json _)
{
	_read_workspace_modules();
	std::vector<slsp::types::HDLModule> ret;

	for (const auto& [path, bb_list] : _cache.get_modules())
	{
		for(const auto& name : bb_list 
			| std::views::transform([](const ModuleBlackBox* p){return p->module_name;}))
			ret.push_back(HDLModule{.file = _cache.get_uri(path).to_string(), .moduleName=name});
	}
	return ret;
}


const std::vector<const ModuleBlackBox*> DiplomatLSP::_h_get_module_bbox(slsp::types::HDLModule params)
{
	const std::string target_file = params.file;
	spdlog::info("Return information for file {}",target_file );
	
	auto* bb_list = _cache.get_bb_by_file(target_file);// _blackboxes.at(lookup_path).get();
	if(bb_list)
	{
		if(params.moduleName.has_value())
		{
			for (const auto* bb : *bb_list) 
			{
				if(bb->module_name == params.moduleName)
					return {bb};
			}

			return {};
		}
		else
		{
			return *bb_list;
		}
	}

	return {};
}

/**
 * @brief Set the top-level module name
 * 
 *	Handler for `diplomat-server.set-top`
 * 
 * @param params an array of size 1 containing only the new top-level module name
 */
void DiplomatLSP::_h_set_top_module(std::optional<std::string> params)
{
	_settings.top_level = params;
	spdlog::info("Set top module {}", _settings.top_level.value_or("UNDEFINED"));
	_compute_project_tree();
	_compile();
}


/**
 * @brief This function shall return the list of files required to build the project.
 * 
 * This list is built using the dependencies scanning capabilities of the {@link VisitorModuleBlackBox}.
 *
 * @param params is a JSON array containing a single {@link slsp::types::HDLModule}
 * @returns a list of URI matching the required files
 */
std::vector<std::string> DiplomatLSP::_h_project_tree_from_module(HDLModule requested_root_module)
{
	_read_workspace_modules();
	uri target_uri(requested_root_module.file);
	
	const ModuleBlackBox* target = nullptr;

	const std::vector<const ModuleBlackBox*> * target_bb_list = _cache.get_bb_by_file(fs::path("/" + target_uri.get_path()));
	if(target_bb_list == nullptr)
	{
		log(slsp::types::MessageType_Error, fmt::format("Unable to retrieve a module from the file {}",requested_root_module.file));
		throw slsp::lsp_request_failed_exception(fmt::format("Unable to retrieve a module from the file {}",requested_root_module.file));
	}
	else
	{
		for(const ModuleBlackBox* bb : *target_bb_list)
		{
			if(! requested_root_module.moduleName || requested_root_module.moduleName.value() == bb->module_name)
			{
				target = bb;
				break;
			}
		}
	}

	if(! target)
	{
		log(slsp::types::MessageType_Error, fmt::format("Unable to find module {} in file {}",requested_root_module.moduleName.value_or("None"), requested_root_module.file));
		throw slsp::lsp_request_failed_exception(fmt::format("Unable to find module {} in file {}",requested_root_module.moduleName.value_or("None"), requested_root_module.file));
	}

	// Here, we have the proper target file.
	// Now, we need to lookup each dependencies.
	std::unordered_set<std::string> processed;
	std::unordered_set<std::string> to_process;
	std::vector<std::string> result;


	result.push_back(_cache.get_uri(_cache.get_file_from_module(target)).to_string());
	for (std::string dep : target->deps)
	{
		to_process.insert(dep);
	}

	while(! to_process.empty())
	{
		const std::string processed_bbname = to_process.extract(to_process.begin()).value();
		if (processed.contains(processed_bbname))
			continue;

		const ModuleBlackBox* processed_bb = _cache.get_bb_by_module(processed_bbname);
		if (!processed_bb)
			continue;

		result.push_back(_cache.get_uri(_cache.get_file_from_module(processed_bb)).to_string());

		for (std::string dep : processed_bb->deps)
		{
			to_process.insert(dep);
		}
	}

	return result;
}

void DiplomatLSP::_h_ignore(std::vector<std::string> params)
{
	//spdlog::info("{}",params);
	for (const std::string& _ : params)
	{
		uri path(_);
		fs::path p = _cache.standardize_path("/" + path.get_path());
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
std::map<std::string,std::optional<Location>> DiplomatLSP::_h_resolve_hier_path(std::vector<std::string> params)
{
	// Params will be a list of hier paths to resolve.
	std::map<std::string,std::optional<Location>> ret;
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
			ret[path] = {};
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
 *
 * @todo define a proper type as a metamodel.
 */
json DiplomatLSP::_h_get_design_hierarchy(json _)
{
	json ret;

	if(! _assert_index())
	{
		return ret;
	}
	const slang::ast::RootSymbol& design_root = _compilation->getRoot();
	HierVisitor hier_visitor(false,_cache.get_uri_bindings());

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


/**
 * @brief Given a design path (such as `dut.scope.foo`) this function will 
 * lookup the scope and return all symbols that shall be visible within this scope.
 * For each symbol, this will associate the range of all references to this symbol.
 *
 * This is particularly used to annotate data from waveforms. 
 * @param params An array with only one string, the hierarchical path of the scope to lookup.
 * @return json A map `symbol_name` to `references_ranges[]`
 */
std::map<std::string,std::vector<slsp::types::Range>> DiplomatLSP::_h_list_symbols(std::string path)
{
	std::map<std::string,std::vector<slsp::types::Range>> ret;
	
	const di::IndexScope* lu_scope = _index->lookup_scope(path);
	
	if(! lu_scope)
	{
		spdlog::warn("Unable to list symbols for scope {}: Scope not found.",path);
		return ret;
	}
	const di::IndexFile* lu_file = _index->get_file(lu_scope->get_source_range().value_or(di::IndexRange()).start.file);// _index->get_file(path);

	if(! lu_file)
	{
		spdlog::warn("Unable to list symbols for file {}: file not found in index.",path);
		return ret;
	}

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

	// spdlog::debug("{}",json(ret).dump(4));

	return ret;
}
