#include <iostream>
#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

#include "nlohmann/json.hpp"
#include "lsp.hpp"
#include "lsp_default_binds.hpp"
#include "rpc_transport.hpp"

#include "types/structs/_InitializeParams.hpp"
#include "types/structs/TextDocumentSyncOptions.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/InitializeResult_serverInfo.hpp"

#include "tcp_interface_server.hpp"

using json = nlohmann::json;
using namespace slsp::types;
void say_hello(slsp::BaseLSP* lsp, json& params)
{
    spdlog::info("Saying hello to {}",params["name"].template get<std::string>());
}

json adder(slsp::BaseLSP* lsp, json& params)
{
    int result = params["a"].template get<int>() + params["b"].template get<int>();
    json ret;
    ret["result"] = result;
    return ret;
}

json initialize(slsp::BaseLSP* lsp, json& params)
{
    _InitializeParams p = params.template get<_InitializeParams>();
    InitializeResult reply;
    reply.serverInfo = InitializeResult_serverInfo{"Slang-LSP","0.0.1"};

    TextDocumentSyncOptions sync;
    sync.openClose = true;
    sync.save = true;
    sync.change = TextDocumentSyncKind::TextDocumentSyncKind_None;
    reply.capabilities.textDocumentSync = sync;
    return reply;
}


void runner(slsp::BaseLSP& lsp)
{
    slsp::perform_default_binds(lsp);
    
    lsp.bind_notification("hello", say_hello);
    lsp.bind_request("add", adder);
    lsp.bind_request("initialize", initialize);
    lsp.run();
    spdlog::info("Clean exit.");
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", "0.0.1");
    prog.add_argument("--tcp")
        .help("Use a TCP connection")
        .default_value(false)
        .implicit_value(true);

    prog.add_argument("--port","-p")
        .help("Port to use")
        .default_value<in_port_t>(8080)
        .scan<'u', in_port_t>();

    try {
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }

    if(prog.get<bool>("--tcp"))
    {
        in_port_t port = prog.get<in_port_t>("--port");
        sockpp::initialize();
        slsp::TCPInterfaceServer itf = slsp::TCPInterfaceServer(port);
        spdlog::info("Await client on port {}...",port);
        itf.await_client();
        
        std::istream tcp_input(&itf);
        std::ostream tcp_output(&itf);

        slsp::BaseLSP lsp(tcp_input,tcp_output);
        runner(lsp);
    }
    else
    {
        slsp::BaseLSP lsp = slsp::BaseLSP();
        runner(lsp);
    }
    return 0;
}
