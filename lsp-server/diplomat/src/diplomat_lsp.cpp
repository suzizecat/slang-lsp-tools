#include "diplomat_lsp.hpp"
#include "spdlog/spdlog.h"

#include "types/structs/SetTraceParams.hpp"
#include "types/structs/_InitializeParams.hpp"
#include "types/structs/InitializeResult.hpp"

using namespace slsp::types;

DiplomatLSP::DiplomatLSP(std::istream &is, std::ostream &os) : BaseLSP(is, os)
{
    
    TextDocumentSyncOptions sync;
    sync.openClose = true;
    sync.save = true;
    sync.change = TextDocumentSyncKind::TextDocumentSyncKind_None;
    capabilities.textDocumentSync = sync;

    _bind_methods();    
}

void DiplomatLSP::_bind_methods()
{
    bind_request("initialize",LSP_MEMBER_BIND(DiplomatLSP,_h_initialize));
    bind_request("diplomat-server.get-modules", LSP_MEMBER_BIND(DiplomatLSP,_h_get_modules));
    bind_request("diplomat-server.get-module-bbox", LSP_MEMBER_BIND(DiplomatLSP,_h_get_module_bbox));
    bind_notification("diplomat-server.full-index", LSP_MEMBER_BIND(DiplomatLSP,hello));
    
    bind_notification("$/setTraceNotification", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));
    bind_notification("$/setTrace", LSP_MEMBER_BIND(DiplomatLSP,_h_setTrace));

    bind_notification("initialized", LSP_MEMBER_BIND(DiplomatLSP,_h_initialized));
    bind_notification("exit", LSP_MEMBER_BIND(DiplomatLSP,_h_exit));
        
    bind_request("shutdown", LSP_MEMBER_BIND(DiplomatLSP,_h_shutdown));
    bind_request("workspace/executeCommand", LSP_MEMBER_BIND(DiplomatLSP,_execute_command_handler));
}

void DiplomatLSP::_read_document(std::string path)
{
    if(_documents.contains(path))
    {
        // Delete previous SVDocument and build it anew.
        _documents.at(path).reset(new SVDocument(path));
    }
    else 
    {
        // Create a brand new object.
        _documents.emplace(path,std::make_unique<SVDocument>(path));
    }
}

void DiplomatLSP::_h_exit(json params)
{
    exit();
}

json DiplomatLSP::_h_initialize(json params)
{
    _InitializeParams p = params.template get<_InitializeParams>();
    InitializeResult reply;
    reply.capabilities = capabilities;
    reply.serverInfo = InitializeResult_serverInfo{"Diplomat-LSP","0.0.1"};

    set_initialized(true) ;
    return reply;
}

void DiplomatLSP::_h_initialized(json params)
{
    spdlog::info("Client initialization complete.");
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

json DiplomatLSP::_h_get_modules(json params)
{

    return R"(
  [
        {
            "file": "instruction_counter.sv",
            "name": "instruction_counter"
        },
        {
            "file": "alu.sv",
            "name": "ALU"
        }
    ])"_json;
}


json DiplomatLSP::_h_get_module_bbox(json _)
{

    json params = _[0];
    spdlog::info("Return information for file {}",params.at("file").template get<std::string>());
    return R"(
  {
            "module": "instruction_counter",
            "parameters": [
                {
                    "default": "8",
                    "name": "K_ADWIDTH",
                    "type": "ImplicitType"
                }
            ],
            "ports": [
                {
                    "kind": "ImplicitType",
                    "name": "i_clk",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "i_rstn",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "i_incr_en",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "o_addr",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "i_set_addr",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "i_jump",
                    "type": "unknown"
                },
                {
                    "kind": "ImplicitType",
                    "name": "i_addr",
                    "type": "unknown"
                }
            ]
        })"_json;
}

void DiplomatLSP::hello(json _)
{
    show_message(MessageType_Info,"Hello there !");
    log(MessageType_Info,"I said hello !");
}