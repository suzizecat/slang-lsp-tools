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
#include "types/structs/DefinitionParams.hpp" 
#include "types/structs/Location.hpp" 

// UNIX only header
#include <wait.h>
#include <fstream>

#include "uri.hh"

using namespace slsp::types;

namespace fs = std::filesystem;


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
    //exit();
    return json();
}


json DiplomatLSP::_h_gotoDefinition(json _)
{
    slsp::types::DefinitionParams params = _;
    slsp::types::Location result;

    fs::path source_path = fs::canonical(fs::path("/" + uri(params.textDocument.uri).get_path()));

    auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1, params.position.character+1);
    if (syntax)
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        slang::SourceRange return_range = _index->get_definition(CONST_TOKNODE_SR(*syntax));
        spdlog::info("Found definition.");

        return _slang_to_lsp_location(return_range);
    }
    else
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        spdlog::info("Definition not found.");
        return {};
    }
}


json DiplomatLSP::_h_references(json _)
{
    slsp::types::DefinitionParams params = _;
    slsp::types::Location result;

    fs::path source_path = fs::canonical(fs::path("/" + uri(params.textDocument.uri).get_path()));

    auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1, params.position.character+1);
    if (syntax)
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        std::vector<slang::SourceRange> return_range = _index->get_references(CONST_TOKNODE_SR(*syntax));
        spdlog::info("Found references.");

        std::vector<Location> result;
        for(const auto& range : return_range)
            result.push_back(_slang_to_lsp_location(range));
        
        return result;
    }
    else
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        spdlog::info("References not found.");
        return {};
    }
}

json DiplomatLSP::_h_rename(json _)
{
    slsp::types::RenameParams params = _;
    slsp::types::WorkspaceEdit result;

    fs::path source_path = fs::canonical(fs::path("/" + uri(params.textDocument.uri).get_path()));

    auto syntax = _index->get_syntax_from_position(source_path, params.position.line +1, params.position.character+1);
    if (syntax)
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Lookup definition from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        std::vector<slang::SourceRange> return_range = _index->get_references(CONST_TOKNODE_SR(*syntax));
        spdlog::info("Perform rename.");

        std::unordered_map<std::string,std::vector<slsp::types::TextEdit>> edits;

        for(const auto& range : return_range)
        {
            slsp::types::Location edit_change = _slang_to_lsp_location(range);
            if (! edits.contains(edit_change.uri))
                edits[edit_change.uri] = {};
            
            edits.at(edit_change.uri).push_back(slsp::types::TextEdit{edit_change.range,params.newName});
        }
        result.changes = edits;
        return result;
    }
    else
    {
        log(slsp::types::MessageType::MessageType_Log, fmt::format("Unable to get a syntax node from position {}:{}:{}",source_path.generic_string(), params.position.line, params.position.character));
        spdlog::info("References not found.");
        throw slsp::lsp_request_failed_exception("Selected area did not returned a significant syntax node.");
    }
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
