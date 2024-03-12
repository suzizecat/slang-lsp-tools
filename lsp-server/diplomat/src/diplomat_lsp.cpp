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

#include "visitor_index.hpp"

// UNIX only header
#include <wait.h>
#include <fstream>

#include "uri.hh"

using namespace slsp::types;

namespace fs = std::filesystem;

bool DiplomatLSP::_assert_index(bool always_throw)
{
    if (_index == nullptr)
    {
        if (!_broken_index_emitted)
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

DiplomatLSP::DiplomatLSP(std::istream &is, std::ostream &os, bool watch_client_pid) : BaseLSP(is, os), 
_this_shared(this),
_sm(new slang::SourceManager()),
_diagnostic_client(new slsp::LSPDiagnosticClient(_documents,_sm.get())),
_watch_client_pid(watch_client_pid)
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

    capabilities.textDocumentSync = sync;
    capabilities.workspace = sc_ws;
    capabilities.definitionProvider = true;
    capabilities.referencesProvider = true;
    capabilities.documentFormattingProvider = true; // Can handle formatting options
    capabilities.renameProvider = true;

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
    result.uri = get_file_uri(_sm->getFullPath(sr.start().buffer())).to_string();
    return result;
}



slang::ast::Compilation* DiplomatLSP::get_compilation()
{
    if (_compilation == nullptr)
        _compile();

    return _compilation.get();
}


void DiplomatLSP::_bind_methods()
{
    bind_request("initialize",LSP_MEMBER_BIND(DiplomatLSP,_h_initialize));
    bind_notification("diplomat-server.index-dump", LSP_MEMBER_BIND(DiplomatLSP,dump_index));
    bind_request("diplomat-server.get-modules", LSP_MEMBER_BIND(DiplomatLSP,_h_get_modules));
    bind_request("diplomat-server.get-module-bbox", LSP_MEMBER_BIND(DiplomatLSP,_h_get_module_bbox));
    bind_notification("diplomat-server.full-index", LSP_MEMBER_BIND(DiplomatLSP,hello));
    bind_notification("diplomat-server.ignore", LSP_MEMBER_BIND(DiplomatLSP,_h_ignore));
    bind_notification("diplomat-server.save-config", LSP_MEMBER_BIND(DiplomatLSP,_h_save_config));
    
    bind_notification("$/setTraceNotification", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));
    bind_notification("$/setTrace", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));

    bind_notification("initialized", LSP_MEMBER_BIND(DiplomatLSP,_h_initialized));
    bind_notification("workspace/didChangeConfiguration", LSP_MEMBER_BIND(DiplomatLSP,_h_update_configuration));
    bind_notification("exit", LSP_MEMBER_BIND(DiplomatLSP,_h_exit));
        
    bind_request("shutdown", LSP_MEMBER_BIND(DiplomatLSP, _h_shutdown));

    bind_notification("textDocument/didOpen", LSP_MEMBER_BIND(DiplomatLSP, _h_didOpenTextDocument));
    bind_notification("textDocument/didSave", LSP_MEMBER_BIND(DiplomatLSP, _h_didSaveTextDocument));
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

void DiplomatLSP::_add_workspace_folders(const std::vector<WorkspaceFolder>& to_add)
{
    for (const WorkspaceFolder& wf : to_add)
    {
        uri path = uri(wf.uri);
        spdlog::info("Add workspace {} ({}) to working directories.", wf.name, wf.uri);
        _root_dirs.push_back(fs::path("/" + path.get_path()));
    }
}

void DiplomatLSP::_remove_workspace_folders(const std::vector<WorkspaceFolder>& to_rm)
{
    for (const WorkspaceFolder& wf : to_rm)
    {
        uri path = uri(wf.uri);
        spdlog::info("Remove workspace {} ({}) to working directories.", wf.name, wf.uri);
        std::remove(_root_dirs.begin(), _root_dirs.end(), "/" + path.get_path());
    }
}

void DiplomatLSP::_read_workspace_modules()
{
    log(MessageType_Info, "Reading workspace");
    namespace fs = fs;
    _documents.clear();
    _sm.reset(new slang::SourceManager());

    fs::path p;
    SVDocument* doc;
    for (const fs::path& root : _root_dirs)
    {
        fs::recursive_directory_iterator it(root);
        for (const fs::directory_entry& file : it)
        {
            if (!file.exists() ||  _settings.excluded_paths.contains(fs::canonical(file.path())))
            {
                if(file.is_directory())
                    it.disable_recursion_pending();
                continue;
            }
                
                
            if (file.is_regular_file() && (_accepted_extensions.contains((p = file.path()).extension())))
            {
                spdlog::debug("Read SV file {}", p.generic_string());
                doc = _read_document(p);
                _module_to_file[doc->get_module_name()] = p.generic_string();
            }
        }
    }
}

void DiplomatLSP::_compile()
{
    spdlog::info("Request design compilation");
        
    
    // As per slang limitation, it is needed to recreate the diagnostic engine
    // because the source manager will be deleted by _read_workspace_modules.
    // Therefore, the client shall also be rebuilt.
    _read_workspace_modules();
    _diagnostic_client.reset(new slsp::LSPDiagnosticClient(_documents,_sm.get(),_diagnostic_client.get()));


    slang::DiagnosticEngine de = slang::DiagnosticEngine(*_sm);
    
    de.setErrorLimit(500);
    de.setIgnoreAllWarnings(false);
    de.setSeverity(slang::diag::MismatchedTimeScales,slang::DiagnosticSeverity::Ignored);
    de.setSeverity(slang::diag::MissingTimeScale,slang::DiagnosticSeverity::Ignored);
    de.addClient(_diagnostic_client);


    slang::ast::CompilationOptions coptions;
    coptions.flags &= ~ (slang::ast::CompilationFlags::SuppressUnused);
    //coptions.flags |= slang::ast::CompilationFlags::IgnoreUnknownModules;
    slang::Bag bag(coptions);
    _compilation.reset(new slang::ast::Compilation(bag));

    spdlog::info("Add syntax trees");
    for (const auto& [key, value] : _documents)
    {
        _compilation->addSyntaxTree(value->st);
    }

    _compilation->getRoot();

    spdlog::info("Issuing diagnostics");
    for (const slang::Diagnostic& diag : _compilation->getAllDiagnostics())
        de.issue(diag);

    spdlog::info("Send diagnostics");
    _emit_diagnostics();

    spdlog::info("Run indexer");
    slsp::IndexVisitor idx_visit(_compilation->getSourceManager());
    try
    {
        _compilation->getRoot().visit(idx_visit);
        _index = std::move(idx_visit.extract_index());
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

void DiplomatLSP::_save_client_uri(const std::string& client_uri)
{
    spdlog::debug("Register raw URI {}.",client_uri);
    fs::path canon_path = fs::canonical("/" + uri(client_uri).get_path());
    std::string abspath = canon_path.generic_string();
    
    // if(_doc_path_to_client_uri.contains(canon_path) && _doc_path_to_client_uri.at(canon_path) == client_uri)
    //     return;

    _doc_path_to_client_uri[canon_path] = client_uri;
    if(_documents.contains(canon_path))
    {
        _documents.at(canon_path)->doc_uri = client_uri;

        if(_diagnostic_client != nullptr)
        {
            if(_diagnostic_client->remap_diagnostic_uri(fmt::format("file://{}",abspath),client_uri))
            {
                _emit_diagnostics();
            }
        }
    }
}

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
    if(_doc_path_to_client_uri.contains(canon_path))
        ret->doc_uri = _doc_path_to_client_uri.at(canon_path);

    return ret;
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
        ofile << std::setw(4) << _index->dump() << std::endl;
        show_message(MessageType_Info, "Index successfully dumped.");
        log(MessageType_Info, fmt::format("Index successfully dumped to {}.", opath.generic_string()));
        spdlog::info("Dumped internal index to {}",opath.generic_string());
    }
}

uri DiplomatLSP::get_file_uri(const std::filesystem::path& path) const
{
    fs::path lu_path = fs::weakly_canonical(path);
    if(_doc_path_to_client_uri.contains(lu_path))
    {
        return uri(_doc_path_to_client_uri.at(lu_path));
    }
    else
    {
        return uri(fmt::format("file://{}",lu_path.generic_string()));
    }
}
