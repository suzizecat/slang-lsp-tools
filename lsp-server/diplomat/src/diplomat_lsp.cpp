#include "diplomat_lsp.hpp"
#include "spdlog/spdlog.h"

#include "types/structs/SetTraceParams.hpp"


#include "slang/diagnostics/DiagnosticEngine.h"
#include "slang/diagnostics/AllDiags.h"

#include "types/structs/InitializeParams.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/DidChangeWorkspaceFoldersParams.hpp" 
#include "types/structs/DidSaveTextDocumentParams.hpp" 



#include "uri.hh"

using namespace slsp::types;

DiplomatLSP::DiplomatLSP(std::istream &is, std::ostream &os) : BaseLSP(is, os), 
_this_shared(this),
_sm(new slang::SourceManager()),
_diagnostic_client(new slsp::LSPDiagnosticClient(_documents))
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

    _bind_methods();    
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
    bind_request("diplomat-server.get-modules", LSP_MEMBER_BIND(DiplomatLSP,_h_get_modules));
    bind_request("diplomat-server.get-module-bbox", LSP_MEMBER_BIND(DiplomatLSP,_h_get_module_bbox));
    bind_notification("diplomat-server.full-index", LSP_MEMBER_BIND(DiplomatLSP,hello));
    bind_notification("diplomat-server.ignore", LSP_MEMBER_BIND(DiplomatLSP,_h_ignore));
    
    bind_notification("$/setTraceNotification", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));
    bind_notification("$/setTrace", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));

    bind_notification("initialized", LSP_MEMBER_BIND(DiplomatLSP,_h_initialized));
    bind_notification("exit", LSP_MEMBER_BIND(DiplomatLSP,_h_exit));
        
    bind_request("shutdown", LSP_MEMBER_BIND(DiplomatLSP, _h_shutdown));

    bind_notification("textDocument/didSave", LSP_MEMBER_BIND(DiplomatLSP, _h_didSaveTextDocument));
    bind_notification("workspace/didChangeWorkspaceFolders", LSP_MEMBER_BIND(DiplomatLSP, _h_didChangeWorkspaceFolders));
    bind_request("workspace/executeCommand", LSP_MEMBER_BIND(DiplomatLSP,_execute_command_handler));
}



void DiplomatLSP::_emit_diagnostics()
{
    if (_diagnostic_client.get() != nullptr)
    {
        _diagnostic_client->_cleanup_diagnostics();
        for(auto [key,value] : _diagnostic_client->get_publish_requests())
            send_notification("textDocument/publishDiagnostics", *value);
    }
}

void DiplomatLSP::_erase_diagnostics()
{
    if (_diagnostic_client.get() != nullptr)
    {
        _diagnostic_client->_clear_diagnostics();
        for (auto [key, value] : _diagnostic_client->get_publish_requests())
            send_notification("textDocument/publishDiagnostics", *value);
    }

}

void DiplomatLSP::_add_workspace_folders(const std::vector<WorkspaceFolder>& to_add)
{
    for (const WorkspaceFolder& wf : to_add)
    {
        uri path = uri(wf.uri);
        spdlog::info("Add workspace {} ({}) to working directories.", wf.name, wf.uri);
        _root_dirs.push_back(std::filesystem::path("/" + path.get_path()));
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
    namespace fs = std::filesystem;
    _documents.clear();
    _sm.reset(new slang::SourceManager());

    fs::path p;
    SVDocument* doc;
    for (const fs::path& root : _root_dirs)
    {
        fs::recursive_directory_iterator it(root);
        for (const fs::directory_entry& file : it)
        {
            if (_excluded_paths.contains(std::filesystem::canonical(file.path())))
            {
                if(file.is_directory())
                    it.disable_recursion_pending();
                continue;
            }
                
                
            if (file.is_regular_file() && (p = file.path()).extension() == ".sv")
            {
                spdlog::info("Read SV file {}", p.generic_string());
                doc = _read_document(p);
                _module_to_file[doc->get_module_name()] = p.generic_string();
            }
        }
    }
}

void DiplomatLSP::_compile()
{
    spdlog::info("Request design compilation");
        
    _read_workspace_modules();
    // As per slang limitation, it is needed to recreate the diagnostic engine
    // because the source manager has been deleted by _read_workspace_modules.
    // Therefore, the client shall also be rebuild.

    _erase_diagnostics();
    _diagnostic_client.reset(new slsp::LSPDiagnosticClient(_documents));

    slang::DiagnosticEngine de = slang::DiagnosticEngine(*_sm);
    de.setErrorLimit(500);
    de.setSeverity(slang::diag::MismatchedTimeScales,slang::DiagnosticSeverity::Ignored);
    de.setSeverity(slang::diag::MissingTimeScale,slang::DiagnosticSeverity::Ignored);
    de.addClient(_diagnostic_client);

    
    _compilation.reset(new slang::ast::Compilation());

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
    spdlog::info("Diagnostic run done.");
}

SVDocument* DiplomatLSP::_read_document(std::filesystem::path path)
{
    std::string canon_path = std::filesystem::canonical(path).generic_string();
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

    return _documents.at(canon_path).get();
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
    _read_document(std::filesystem::path("/" + uri(param.textDocument.uri).get_path()));
    _compile();
}

void DiplomatLSP::_h_exit(json params)
{
    exit();
}

json DiplomatLSP::_h_initialize(json params)
{
    InitializeParams p = params.template get<InitializeParams>();
    bool got_workspace = false;

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
            _root_dirs.push_back(std::filesystem::path(uri(p.rootUri.value()).get_path()));
        }
        else if(p.rootPath)
        {
            spdlog::info("Add root directory from path: {}", p.rootPath.value());
            _root_dirs.push_back(std::filesystem::path(p.rootPath.value()));
        }
        
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
    _compile();
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
    return json();
}

void DiplomatLSP::_h_set_top_module(json _)
{
    _top_level = _[0].at("top").template get<std::string>();
}

json DiplomatLSP::_h_get_modules(json params)
{
    _read_workspace_modules();
    json ret = json::array();

    for (auto item : _module_to_file)
    {
        ret.push_back({ {"name", item.first},{"file", item.second} });
    }
    return ret;
}


json DiplomatLSP::_h_get_module_bbox(json _)
{

    json params = _[0];
    const std::string target_file = params.at("file").template get<std::string>();
    spdlog::info("Return information for file {}",target_file );
    SVDocument* doc = _documents.at(target_file).get();
    
    spdlog::info("Got {} IOs on the module {}.", doc->bb.value().ports.size(), doc->bb.value().module_name);
    return doc->bb.value();
}

void DiplomatLSP::_h_ignore(json params)
{
    for (const json& record : params.at(1))
    {
        std::filesystem::path p = std::filesystem::canonical(record["path"].template get<std::string>());
        spdlog::info("Ignore path {}", p.generic_string());
        _excluded_paths.insert(p);
    }
}

void DiplomatLSP::hello(json _)
{
    show_message(MessageType_Info,"Hello there !");
    log(MessageType_Info,"I said hello !");
}