#include <iostream>
#include <memory>
#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "fmt/format.h"

#include "nlohmann/json.hpp"
#include "diplomat_lsp.hpp"
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
    std::string name = params["name"].template get<std::string>();
    lsp->show_message(MessageType::MessageType_Info,fmt::format("Hello {} !",name));
    spdlog::info("Saying hello to {}",name);
}

void test_function(slsp::BaseLSP* lsp, json& params)
{
    lsp->show_message(MessageType::MessageType_Info,"Hello world !");
    lsp->log(MessageType::MessageType_Info,"Somebody required to say hello.");
    spdlog::info("Hello world !");
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
    reply.capabilities = lsp->capabilities;
    reply.serverInfo = InitializeResult_serverInfo{"Slang-LSP","0.0.1"};

    TextDocumentSyncOptions sync;
    sync.openClose = true;
    sync.save = true;
    sync.change = TextDocumentSyncKind::TextDocumentSyncKind_None;
    reply.capabilities.textDocumentSync = sync;

    lsp->set_initialized(true) ;
    return reply;
}


void runner(slsp::BaseLSP& lsp)
{
    slsp::perform_default_binds(lsp);
    
    // lsp.bind_notification("hello", say_hello);
    // lsp.bind_notification("diplomat-server.full-index", test_function);
    // lsp.bind_request("add", adder);
    //lsp.bind_request("initialize", initialize);
    lsp.run();
    spdlog::info("Clean exit.");
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", "0.0.1");
    prog.add_argument("--tcp")
        .help("Use a TCP connection")
        .default_value(false)
        .implicit_value(true);
    prog.add_argument("--verbose")
        .help("Increase verbosity")
        .default_value(false)
        .implicit_value(true);
    prog.add_argument("--verbose-trace")
        .help("Increase verbosity")
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


    if(prog.get<bool>("--verbose"))
    {
        spdlog::set_level(spdlog::level::debug);
    }

    if(prog.get<bool>("--verbose-trace"))
    {
        spdlog::set_level(spdlog::level::trace);
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

        DiplomatLSP lsp(tcp_input,tcp_output);
        runner(lsp);
    }
    else
    {
        auto logger =  spdlog::basic_logger_mt("main","./diplomat-lsp.log");
        logger->set_level(spdlog::get_level());
        logger->flush_on(logger->level());
        logger->info("Start new log.");
        spdlog::set_default_logger(logger);

        DiplomatLSP lsp = DiplomatLSP();
        lsp.set_watch_client_pid(false);
        runner(lsp);
    }
    return 0;
}
