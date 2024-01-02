#include "diplomat_lsp.hpp"
#include "spdlog/spdlog.h"

#include <chrono>
#include "types/structs/SetTraceParams.hpp"

#include "slang/diagnostics/AllDiags.h"

#include "types/structs/InitializeParams.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/ConfigurationParams.hpp"
#include "types/structs/ConfigurationItem.hpp"
#include "types/structs/DidChangeWorkspaceFoldersParams.hpp" 
#include "types/structs/DidOpenTextDocumentParams.hpp" 
#include "types/structs/DidSaveTextDocumentParams.hpp" 
#include "types/structs/RegistrationParams.hpp" 
#include "types/structs/Registration.hpp" 

// UNIX only header
#include <wait.h>
#include <fstream>

#include "uri.hh"

using namespace slsp::types;

namespace fs = std::filesystem;

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
    bind_notification("diplomat-server.save-config", LSP_MEMBER_BIND(DiplomatLSP,_h_save_config));
    
    bind_notification("$/setTraceNotification", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));
    bind_notification("$/setTrace", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));

    bind_notification("initialized", LSP_MEMBER_BIND(DiplomatLSP,_h_initialized));
    bind_notification("workspace/didChangeConfiguration", LSP_MEMBER_BIND(DiplomatLSP,_h_update_configuration));
    bind_notification("exit", LSP_MEMBER_BIND(DiplomatLSP,_h_exit));
        
    bind_request("shutdown", LSP_MEMBER_BIND(DiplomatLSP, _h_shutdown));

    bind_notification("textDocument/didOpen", LSP_MEMBER_BIND(DiplomatLSP, _h_didOpenTextDocument));
    bind_notification("textDocument/didSave", LSP_MEMBER_BIND(DiplomatLSP, _h_didSaveTextDocument));
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
            if (_settings.excluded_paths.contains(fs::canonical(file.path())))
            {
                if(file.is_directory())
                    it.disable_recursion_pending();
                continue;
            }
                
                
            if (file.is_regular_file() && (p = file.path()).extension() == ".sv")
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
    // Therefore, the client shall also be rebuild.
    _erase_diagnostics();
    _read_workspace_modules();
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

void DiplomatLSP::_save_client_uri(const std::string& client_uri)
{
    spdlog::debug("Register raw URI {}.",client_uri);
    std::string abspath = fs::canonical("/" + uri(client_uri).get_path()).generic_string();
    _doc_path_to_client_uri[abspath] = client_uri;
    if(_documents.contains(abspath))
    {
        _documents.at(abspath)->doc_uri = client_uri;

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
    std::string canon_path = fs::canonical(path).generic_string();
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
            _root_dirs.push_back(fs::path(uri(p.rootUri.value()).get_path()));
        }
        else if(p.rootPath)
        {
            spdlog::info("Add root directory from path: {}", p.rootPath.value());
            _root_dirs.push_back(fs::path(p.rootPath.value()));
        }
        
    }

    if (p.processId)
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

    send_notification("client/registerCapability",p);
    

    slsp::types::ConfigurationItem conf_path;
    conf_path.section = "diplomatServer.server.configurationPath";

    slsp::types::ConfigurationParams conf_request;
    conf_request.items.push_back(conf_path);
    
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
    return json();
}

void DiplomatLSP::_h_save_config(json params)
{
    spdlog::info("Write configuration to {}",fs::canonical(_settings_path).generic_string());
    log(slsp::types::MessageType::MessageType_Info,fmt::format("Write configuration to {}",_settings_path.generic_string()));
    std::ofstream out(_settings_path);
    json j = _settings;
    out << j.dump(4);
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
    std::string lookup_path = fs::canonical(target_file).generic_string();
    
    SVDocument* doc = _documents.at(lookup_path).get();
    return doc->bb.value();
}

void DiplomatLSP::_h_ignore(json params)
{
    for (const json& record : params.at(1))
    {
        fs::path p = fs::canonical(record["path"].template get<std::string>());
        spdlog::info("Ignore path {}", p.generic_string());
        _settings.excluded_paths.insert(p);
    }
}

void DiplomatLSP::_h_get_configuration(json &clientinfo)
{
    _settings_path = fs::path(clientinfo[0]);
    
    if(fs::exists(_settings_path))
    {
        spdlog::info("Read configuration file {}",_settings_path.generic_string());
        std::ifstream conf_file(_settings_path);
        json conf = json::parse(conf_file) ;
        _settings = conf.template get<slsp::DiplomatLSPWorkspaceSettings>();
    }
}

void DiplomatLSP::_h_get_configuration_on_init(json &clientinfo)
{
    _h_get_configuration(clientinfo);
    _compile();
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

void DiplomatLSP::hello(json _)
{
    show_message(MessageType_Info,"Hello there !");
    log(MessageType_Info,"I said hello !");
}