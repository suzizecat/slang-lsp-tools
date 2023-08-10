#include <iostream>
#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

#include "nlohmann/json.hpp"
#include "lsp.hpp"
#include "rpc_transport.hpp"

#include "types/structs/_InitializeParams.hpp"
#include "types/structs/TextDocumentSyncOptions.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/InitializeResult_serverInfo.hpp"

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


void lsp_shutdown(slsp::BaseLSP* lsp, json& params)
{
    lsp->shutdown();
}

void lsp_exit(slsp::BaseLSP* lsp, json& params)
{
    lsp->exit();
}

void runner()
{
    slsp::BaseLSP lsp;
    lsp.bind_notification("hello", say_hello);
    lsp.bind_notification("exit", lsp_exit);
    lsp.bind_request("add", adder);
    lsp.bind_request("initialize", initialize);
    lsp.run();
    spdlog::info("Clean exit.");
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", "0.0.1");
    try{
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }
    runner();
    return 0;
}
