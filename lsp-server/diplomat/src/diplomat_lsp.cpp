#include "diplomat_lsp.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include <stdexcept>
#include "types/structs/SetTraceParams.hpp"

#include "slang/diagnostics/AllDiags.h"
#include "slang/util/Bag.h"

#include "types/structs/InitializeParams.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/ConfigurationParams.hpp"
#include "types/structs/ConfigurationItem.hpp"
#include "types/structs/DidChangeWorkspaceFoldersParams.hpp" 
#include "types/structs/DidOpenTextDocumentParams.hpp" 
#include "types/structs/DidSaveTextDocumentParams.hpp" 
#include "types/structs/RegistrationParams.hpp" 
#include "types/structs/Registration.hpp" 

#include "index_visitor.hpp"
#include "index_reference_visitor.hpp"

// UNIX only header
#include <sys/wait.h>
#include <fstream>

#include "uri.hh"

using namespace slsp::types;

namespace fs = std::filesystem;


/**
 * @brief Checks that the index is in a working state and may be used.
 * 
 * @param always_throw if set, throw on each call instead of the first failing one
 * @return true if the index is in a working state
 * @return false otherwise.
 */
bool DiplomatLSP::_assert_index(bool always_throw)
{
    if (_index == nullptr)
    {
        if (!_broken_index_emitted || always_throw)
        {
            _broken_index_emitted = true;
            throw slsp::lsp_request_failed_exception(
                "Request failed due to broken index."
                "Try fixing any diagnostic before re - running.");
            
        }
        log(slsp::types::MessageType::MessageType_Error, "Request failed due to broken index.");
        return false;
    }
    return true;
}

/**
 * @brief Construct a new Diplomat LSP::DiplomatLSP object
 * 
 * @param is Input stream used for low level communication with the client.
 * @param os Output stream used for low level communication with the client.
 * @param watch_client_pid If set, should exit/kill the server when the watced PID stop running.
 */
DiplomatLSP::DiplomatLSP(std::istream &is, std::ostream &os, bool watch_client_pid) : BaseLSP(is, os), 
_sm(new slang::SourceManager()),
_diagnostic_client(new slsp::LSPDiagnosticClient(_documents,_sm.get())),
_watch_client_pid(watch_client_pid),
_project_tree_files{},
_project_tree_modules{},
_project_file_tree_valid(false),
_broken_index_emitted(true)
{
    
    TextDocumentSyncOptions sync;
    sync.openClose = true;
    sync.save = true;
    sync.change = TextDocumentSyncKind::TextDocumentSyncKind_None;

    WorkspaceFoldersServerCapabilities ws;
    ws.supported = true;
    ws.changeNotifications = true;

    ServerCapabilities_workspace sc_ws;
    sc_ws.workspaceFolders = ws;

    CompletionOptions sc_completion;
    sc_completion.resolveProvider = false;

    capabilities.textDocumentSync = sync;
    capabilities.workspace = sc_ws;
    capabilities.definitionProvider = true;
    capabilities.referencesProvider = true;
    capabilities.documentFormattingProvider = true; // Can handle formatting options
    capabilities.renameProvider = true;
    capabilities.completionProvider = sc_completion;

    _bind_methods();    
}

/**
 * @brief Convert a slang `SourceRange` to a LSP `Location` object.
 * 
 * @param sr Source range to convert
 * @return slsp::types::Location that matches.
 */
slsp::types::Location DiplomatLSP::_slang_to_lsp_location(const slang::SourceRange& sr) const
{
    slsp::types::Location result;
    result.range.start.line      = _sm->getLineNumber(sr.start()) -1;
    result.range.start.character = _sm->getColumnNumber(sr.start()) -1;
    
    result.range.end.line      = _sm->getLineNumber(sr.end()) -1;
    result.range.end.character = _sm->getColumnNumber(sr.end()) -1;
    //result.uri = fmt::format("file://{}", fs::canonical(_sm->getFullPath(sr.start().buffer())).generic_string());
    result.uri = _cache.get_uri(_sm->getFullPath(sr.start().buffer())).to_string();
    return result;
}


/**
 * @brief Always return a slang compilation of the workspace.
 * If no compilation has been done, run the compilation beforehand.
 * 
 * @return slang::ast::Compilation* compilation object for the workspace. 
 */
slang::ast::Compilation* DiplomatLSP::get_compilation()
{
    if (_compilation == nullptr)
        _compile();

    return _compilation.get();
}

/**
 * @brief Binds all LSP methods to an internal handle (_h_* function)
 * This also registers the commands within the capabilities of the LSP
 * which are sent back to the client. 
 * 
 */
void DiplomatLSP::_bind_methods()
{
    bind_request("initialize",LSP_MEMBER_BIND(DiplomatLSP,_h_initialize));
    bind_notification("diplomat-server.index-dump", LSP_MEMBER_BIND(DiplomatLSP,dump_index));
    bind_request("diplomat-server.get-modules", LSP_MEMBER_BIND(DiplomatLSP,_h_get_modules));
    bind_request("diplomat-server.get-module-bbox", LSP_MEMBER_BIND(DiplomatLSP,_h_get_module_bbox));
    bind_notification("diplomat-server.full-index", LSP_MEMBER_BIND(DiplomatLSP,_h_force_clear_index));
    bind_notification("diplomat-server.ignore", LSP_MEMBER_BIND(DiplomatLSP,_h_ignore));
    bind_notification("diplomat-server.add-incdir", LSP_MEMBER_BIND(DiplomatLSP,_h_add_to_include));
    //bind_notification("diplomat-server.save-config", LSP_MEMBER_BIND(DiplomatLSP,_h_save_config));
    bind_notification("diplomat-server.push-config", LSP_MEMBER_BIND(DiplomatLSP,_h_push_config));
    bind_request("diplomat-server.pull-config", LSP_MEMBER_BIND(DiplomatLSP,_h_pull_config));
    bind_notification("diplomat-server.set-top", LSP_MEMBER_BIND(DiplomatLSP,_h_set_module_top));
    
    bind_notification("diplomat-server.prj.set-project",LSP_MEMBER_BIND(DiplomatLSP,_h_set_project));

    bind_request("diplomat-server.resolve-paths", LSP_MEMBER_BIND(DiplomatLSP,_h_resolve_hier_path));
    bind_request("diplomat-server.get-hierarchy", LSP_MEMBER_BIND(DiplomatLSP,_h_get_design_hierarchy));
    bind_request("diplomat-server.list-symbols", LSP_MEMBER_BIND(DiplomatLSP,_h_list_symbols));

    bind_notification("$/setTraceNotification", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));
    bind_notification("$/setTrace", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));

    bind_notification("initialized", LSP_MEMBER_BIND(DiplomatLSP,_h_initialized));
    bind_notification("workspace/didChangeConfiguration", LSP_MEMBER_BIND(DiplomatLSP,_h_update_configuration));
    bind_notification("exit", LSP_MEMBER_BIND(DiplomatLSP,_h_exit));
        
    bind_request("shutdown", LSP_MEMBER_BIND(DiplomatLSP, _h_shutdown));

    bind_notification("textDocument/didClose", LSP_MEMBER_BIND(DiplomatLSP, _h_didCloseTextDocument));
    bind_notification("textDocument/didOpen", LSP_MEMBER_BIND(DiplomatLSP, _h_didOpenTextDocument));
    bind_notification("textDocument/didSave", LSP_MEMBER_BIND(DiplomatLSP, _h_didSaveTextDocument));
    bind_request("textDocument/completion", LSP_MEMBER_BIND(DiplomatLSP, _h_completion));
    bind_request("textDocument/definition", LSP_MEMBER_BIND(DiplomatLSP, _h_gotoDefinition));
    bind_request("textDocument/formatting", LSP_MEMBER_BIND(DiplomatLSP, _h_formatting));
    bind_request("textDocument/references", LSP_MEMBER_BIND(DiplomatLSP, _h_references));
    bind_request("textDocument/rename", LSP_MEMBER_BIND(DiplomatLSP, _h_rename));
    bind_notification("workspace/didChangeWorkspaceFolders", LSP_MEMBER_BIND(DiplomatLSP, _h_didChangeWorkspaceFolders));
    bind_request("workspace/executeCommand", LSP_MEMBER_BIND(DiplomatLSP,_execute_command_handler));
}


/**
 * @brief Send diagnostics for display to the client
 * 
 * In vscode, this is shown as "Problems" of the workspace. 
 */
void DiplomatLSP::_emit_diagnostics()
{
    if (_diagnostic_client.get() != nullptr)
    {
        for(auto& [key,value] : _diagnostic_client->get_publish_requests())
            send_notification("textDocument/publishDiagnostics", *value);

        // It is required to send "empty" diagnostics to clear up stale diagnostics
        // on the client side.
        // Therefore, cleanup after sending.
        _diagnostic_client->_cleanup_diagnostics();
    }
}

/**
 * @brief Delete all diagnostics and immediately send the erase to the client.
 * 
 */
void DiplomatLSP::_erase_diagnostics()
{
    if (_diagnostic_client.get() != nullptr)
    {
        _diagnostic_client->_clear_diagnostics();
        _emit_diagnostics();
    }

}

diplomat::index::IndexLocation DiplomatLSP::_lsp_to_index_location(const slsp::types::TextDocumentPositionParams& loc)
{
    fs::path source_path = fs::path("/" + uri(loc.textDocument.uri).get_path());
	return diplomat::index::IndexLocation(source_path,loc.position.line +1, loc.position.character +1);
}

slsp::types::Location DiplomatLSP::_index_range_to_lsp(const diplomat::index::IndexRange& loc) const
{
    slsp::types::Location result;
    result.range.start.line      = loc.start.line -1;
    result.range.start.character = loc.start.column -1;
    
    result.range.end.line      = loc.end.line -1;
    result.range.end.character = loc.end.column -1;
    result.uri = _cache.get_uri(loc.start.file).to_string();
    return result;
}

/**
 * @brief Add folders to Diplomat's workspace, provided by the LSP client.
 * Those folders will be scanned for source files.
 *
 * @param to_add list of WorkspaceFolders, provided by a client, to be included in the workspace.
 */
void DiplomatLSP::_add_workspace_folders(const std::vector<WorkspaceFolder>& to_add)
{
    for (const WorkspaceFolder& wf : to_add)
    {
        uri path = uri(wf.uri);
        spdlog::info("Add workspace {} ({}) to working directories.", wf.name, wf.uri);
        _settings.workspace_dirs.emplace(fs::path("/" + path.get_path()));
    }
}

/**
 * @brief Removes a list of workspace folders from the registered workspace directory.
 * 
 * @param to_rm Workspaces to remove.
 */
void DiplomatLSP::_remove_workspace_folders(const std::vector<WorkspaceFolder>& to_rm)
{
    for (const WorkspaceFolder& wf : to_rm)
    {
        uri path = uri(wf.uri);
        spdlog::info("Remove workspace {} ({}) to working directories.", wf.name, wf.uri);
        _settings.workspace_dirs.erase( "/" + path.get_path());
    }
}


/**
 * @brief Read the whole workspace to extract the modules names and blackbox definitions.
 * Does not elaborate, nor generate the index, diagnostics and such.
 */
void DiplomatLSP::_read_workspace_modules()
{
    log(MessageType_Info, "Reading workspace");
    namespace fs = fs;
    _documents.clear();
    _blackboxes.clear();
    _module_to_file.clear();
    
    _sm.reset(new slang::SourceManager());
    for(auto& path : _settings.includes.user)
        _sm->addUserDirectories(path);
    for(auto& path : _settings.includes.system)
        _sm->addUserDirectories(path);

    fs::path p;
    SVDocument* doc;

    for (const fs::path& root : _settings.workspace_dirs)
    {
        fs::recursive_directory_iterator it(root);
        for (const fs::directory_entry& file : it)
        {
            bool skipped = false;
            
            // Skip if the file does not actually exists (broken symlink, for example)
            if (!file.exists())
            {
                skipped = true;    
            }
           
            // canonical shall not be called on a non-existing path.
            if(! skipped)
            {
                std::string path = fs::canonical(file.path()).generic_string();
                
                // Check for explicitely excluded paths (fast)
                if (_settings.excluded_paths.contains(path))
                    skipped = true;
                
                //! Check, if needed, for the exclusion patterns.
                if (!skipped)
                {
                    for(const std::regex& rgx : _settings.excluded_regexs)
                    {   
                        if (std::regex_match(path,rgx))
                        {
                            skipped = true;
                            break;
                        }
                    }
                }
            }
            
            if(skipped)
            {
                if(file.is_directory())
                    it.disable_recursion_pending();
                continue;
            }
            

            if (file.is_regular_file() && (_accepted_extensions.contains((p = file.path()).extension())))
            {
                spdlog::debug("Read SV file {}", p.generic_string());
                doc = _read_document(p);
                _blackboxes.emplace(p,doc->extract_blackbox());

                for(auto& name : std::views::keys(*(_blackboxes.at(p))))
                {
                    // If module to file already have a reference,
                    // try to select the one where the filename match the module name.
                    if(_module_to_file.contains(name))
                    {
                        if(p.has_stem() && p.stem().generic_string() == name)
                            _module_to_file[name] = p;
                        else
                            spdlog::warn("Ignored file {} for module {} as the filename does not match the module and we already have a reference.",
                            p.generic_string(),name);
                    }
                    else
                    {
                        _module_to_file[name] = p;
                    }
                }
            }
        }
    }
}


/**
 * @brief Read and refresh the blackboxes of the project filetree, considering only the files
 * in the project (and not the whole worktree).
 * 
 * Thus, allowing a partial and faster refresh of the blackbox views.
 */
void DiplomatLSP::_read_filetree_modules()
{
    log(MessageType_Info, "Reading filetree");
    namespace fs = fs;
    // Explicitely do NOT clear the blackbox list in order to keep what was done on 
    // global pass and attempts to relink later.
    _documents.clear();
    
    _sm.reset(new slang::SourceManager());
    for(auto& path : _settings.includes.user)
        _sm->addUserDirectories(path);
    for(auto& path : _settings.includes.system)
        _sm->addUserDirectories(path);


    SVDocument* doc;
    for (const fs::path& file : _project_tree_files)
    {                
        spdlog::debug("Read SV file from project tree {}", file.generic_string());
        doc = _read_document(file);
        _blackboxes[file] = doc->extract_blackbox();

        for(auto& module_name : std::views::keys(*(_blackboxes.at(file))))
            _module_to_file[module_name] = file.generic_string();
    }
}


/**
 * @brief Retrieve the SVDocument from a module name if it exists.
 * 
 * @param module name to lookup.
 * @return const SVDocument* Pointer to the SVDocument found if any, nullptr otherwise.
 */
const SVDocument* DiplomatLSP::_document_from_module(const std::string& module) const
{
    try
    {
        return _documents.at(_module_to_file.at(module)).get();
    }
    catch(const std::out_of_range& e)
    {
        spdlog::warn("No document found when looking up the module {}",module);
        return nullptr;
    }
}

/**
 * @brief Retrive the blackbox definition associated to a module name if any.
 * 
 * @param module Module name to lookup.
 * @return const ModuleBlackBox* Blackbox definition found if any, nullptr otherwise.
 */
const ModuleBlackBox* DiplomatLSP::_bb_from_module(const std::string& module) const
{
    try
    {
        return _blackboxes.at(_module_to_file.at(module))->at(module).get();
    }
    catch(const std::out_of_range& e)
    {
        spdlog::warn("No document found when looking up the module {}",module);
        return nullptr;
    }
}

/**
 * @brief Rebuild the project module and file trees
 */
void DiplomatLSP::_compute_project_tree(bool keep_tree)
{
    spdlog::info("Rebuild project file tree");
    if(! keep_tree)
        _clear_project_tree();
    // A top level shall be set beforehand.
    if(! _settings.top_level)
        return;
    _add_module_to_project_tree(_settings.top_level.value());
    _project_file_tree_valid = true;
}

/**
 * @brief Erase the project file and modules tree.
 */
void DiplomatLSP::_clear_project_tree() 
{
    _project_file_tree_valid = false;
    _project_tree_files.clear(); 
    _project_tree_modules.clear();
}

/**
 * @brief Add a given module to the project tree, and all its dependencies.
 * This is a recursive function used to build the whole filetree of the project.
 * 
 * @param mod name of the module to add to the project tree.
 */
void DiplomatLSP::_add_module_to_project_tree(const std::string& mod)
{
    // The workspace modules list shall have been computed beforehand.
    // Select the SVDocument from the module name to retrieve its dependencies.
    const ModuleBlackBox* bb = _bb_from_module(mod);

    // No problem with re-inserting as _project_tree_modules is a set.
    _project_tree_modules.insert(mod);

    // Current document could be nullptr if the required module has not been found.
    // In this case it is just skipped. 
    if(bb != nullptr)
    {
        _project_tree_files.insert(_module_to_file.at(mod));

        for(const std::string_view& _ : bb->deps)
        {
            // Convert string view to string to not depend on the buffer.
            std::string dep(_);
            if(!_project_tree_modules.contains(dep))
            {
                _add_module_to_project_tree(dep);
            }
        }
    }
}

/**
 * @brief Performs all steps of the compilation and elaboration of the design, 
 * including diagnostic emission.
 */
void DiplomatLSP::_compile()
{
    spdlog::info("Request design compilation");
        
    if(!_project_file_tree_valid)
        _read_workspace_modules();
    else
        _read_filetree_modules();

    // As per slang limitation, it is needed to recreate the diagnostic engine
    // because the source manager will be deleted by _read_workspace_modules.
    // Therefore, the client shall also be rebuilt.
    _diagnostic_client.reset(new slsp::LSPDiagnosticClient(_documents,_sm.get(),_diagnostic_client.get()));

    slang::DiagnosticEngine de = slang::DiagnosticEngine(*_sm);
    
    de.setErrorLimit(500);
    de.setIgnoreAllWarnings(false);
    de.setSeverity(slang::diag::MismatchedTimeScales,slang::DiagnosticSeverity::Ignored);
    de.setSeverity(slang::diag::MissingTimeScale,slang::DiagnosticSeverity::Ignored);
    de.setSeverity(slang::diag::UnusedDefinition,slang::DiagnosticSeverity::Ignored);
    de.addClient(_diagnostic_client);

    slang::ast::CompilationOptions coptions;
    coptions.flags &= ~ (slang::ast::CompilationFlags::SuppressUnused);
    
    if (_settings.top_level)
        coptions.topModules = {_settings.top_level.value()};
    //coptions.flags |= slang::ast::CompilationFlags::IgnoreUnknownModules;
    slang::Bag bag(coptions);

    // Regenerate compilation object to allow for a "recompilation".
    _compilation.reset(new slang::ast::Compilation(bag));

    if(_settings.top_level)
    {
        // Always rebuild the project tree.
        _compute_project_tree();
        spdlog::info("Add syntax trees from project file tree");
        for (const auto& file : _project_tree_files)
        {
            _compilation->addSyntaxTree(_documents.at(file)->st);
        }
    }
    else
    {
        spdlog::info("Add syntax trees from workspace");    
        for (const auto& [key, value] : _documents)
        {
            _compilation->addSyntaxTree(value->st);
        }
    }

    // Actually compile and elaborate the design
    _compilation->getRoot();

    spdlog::info("Issuing diagnostics");
    for (const slang::Diagnostic& diag : _compilation->getAllDiagnostics())
        de.issue(diag);

    spdlog::info("Send diagnostics");
    _emit_diagnostics();

    spdlog::info("Run indexer");
    
   diplomat::index::IndexVisitor idx_visit(_compilation->getSourceManager());
    try
    {
        spdlog::info("Processing symbols and hierarchy");
        _compilation->getRoot().visit(idx_visit);
        _index = std::move(idx_visit.get_index());
        spdlog::info("Processing references");

        for(const auto& file : _index->get_indexed_files())
        {
            {
                spdlog::info("Processing references for {}",file->get_path().generic_string());

                auto stx = file->get_syntax_root();
                if(stx)
                {
                    diplomat::index::ReferenceVisitor ref_visitor(_compilation->getSourceManager(),_index.get());
                    stx->visit(ref_visitor);
                }
            }
        }

        if(_broken_index_emitted)
        {
            log(MessageType_Info, "Index restored");
            _broken_index_emitted = false;
        }
    }
    catch(const std::runtime_error & e)
    {
        _index.reset();
        spdlog::error("Indexing error {}", e.what());
    }
    spdlog::info("Compilation done.");
}


/**
 * @brief Store an URI, provided by the client, for a workspace file.
 * This will be used to provide the right location in diagnostics, thus avoiding
 * opening twice the same file due to mismatching paths.
 * 
 * @param client_uri The URI to store.
 */
void DiplomatLSP::_save_client_uri(const std::string& client_uri)
{
    spdlog::debug("Register raw URI {}.",client_uri);
    
    fs::path canon_path = _cache.record_uri(uri(client_uri));

    // fs::path orig_path = fs::path("/" + uri(client_uri).get_path());
    // if(! fs::exists(orig_path))
    //     return;

    // fs::path canon_path = fs::canonical(orig_path);

    std::string abspath = canon_path.generic_string();
    if(! fs::exists(abspath))
        return;
    
     if(_diagnostic_client != nullptr)
    {
        if(_diagnostic_client->remap_diagnostic_uri(fmt::format("file://{}",abspath),client_uri))
        {
            _emit_diagnostics();
        }
    }

    // _doc_path_to_client_uri[canon_path] = client_uri;
    // if(_documents.contains(canon_path))
    // {
    //     _documents.at(canon_path)->doc_uri = client_uri;

       
    // }
}

/**
 * @brief Read a source file and return the generated SVDocument.
 * 
 * @param path path of the file to read
 * @return SVDocument* Representation of the file read.
 */
SVDocument* DiplomatLSP::_read_document(fs::path path)
{
    std::string canon_path = fs::canonical(path);
    if (_documents.contains(canon_path))
    {
        // Delete previous SVDocument and build it anew.
        _documents.at(canon_path).reset(new SVDocument(canon_path,_sm.get()));
    }
    else 
    {
        // Create a brand new object.
        _documents[canon_path] = std::make_unique<SVDocument>(canon_path,_sm.get());
    }

    SVDocument* ret = _documents.at(canon_path).get();

    // Rebind the file path if an URI is already registered.
    ret->doc_uri = _cache.get_uri(path).to_string();

    // if(_doc_path_to_client_uri.contains(canon_path))
    //     ret->doc_uri = _doc_path_to_client_uri.at(canon_path);

    return ret;
}


/**
 * @brief Set the top-level module (by name).
 * This will force a recompilation of the design.
 * 
 * @param new_top Module name of the top level.
 */
void DiplomatLSP::set_top_level(const std::string& new_top)
{
    _settings.top_level = new_top;
    _compile();
}


void DiplomatLSP::hello(json _)
{
    show_message(MessageType_Info,"Hello there !");
    log(MessageType_Info,"I said hello !");
}

void DiplomatLSP::dump_index(json _)
{
    if(! _index)
    {
        show_message(MessageType_Error, "No index is managed, dump failed.");
        log(MessageType_Error, "Index pointer does not manage anything. Dump aborted.");
    }
    else
    {
        fs::path opath = fs::absolute("./index_dump.json");
        std::ofstream ofile(opath);
        ofile << std::setw(4) << _index->dump().dump(4) << std::endl;
        show_message(MessageType_Info, "Index successfully dumped.");
        log(MessageType_Info, fmt::format("Index successfully dumped to {}.", opath.generic_string()));
        spdlog::info("Dumped internal index to {}",opath.generic_string());
    }
}
