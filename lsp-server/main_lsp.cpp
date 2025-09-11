#include <iostream>
#include <memory>
#include "argparse/argparse.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "fmt/format.h"

#include "nlohmann/json.hpp"
#include "diplomat_lsp.hpp"
#include "lsp_spdlog_sink.hpp"
#include "lsp_default_binds.hpp"
#include "rpc_transport.hpp"

#include "types/structs/_InitializeParams.hpp"
#include "types/structs/TextDocumentSyncOptions.hpp"
#include "types/structs/InitializeResult.hpp"
#include "types/structs/InitializeResult_serverInfo.hpp"

#include "tcp_interface_server.hpp"

#ifndef DIPLOMAT_VERSION_STRING
#define DIPLOMAT_VERSION_STRING "custom-build"
#endif


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
    
    lsp.run();
    spdlog::info("Clean exit.");
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", DIPLOMAT_VERSION_STRING );
    prog.add_argument("--tcp")
        .help("Use a TCP connection")
        .default_value(false)
        .implicit_value(true);
    prog.add_argument("--allow-reboot")
        .help("When using a TCP connection, allow reboot of the server")
        .default_value(false)
        .implicit_value(true);
     prog.add_argument("--forward-log")
        .help("Forward the logs to the client")
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
    prog.add_argument("-l", "--log")
        .default_value(std::string{"./diplomat-lsp.log"})
        .help("Set the log file");


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
    
    // Setup SPDLOG
    auto fwd_sink = std::make_shared<slsp::lsp_spdlog_sink_mt>();

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(prog.get<std::string>("--log"),true);
    file_sink->set_level(spdlog::get_level());

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::get_level());

    auto logger = std::make_shared<spdlog::logger>("main");
    logger->set_level(spdlog::get_level());
    logger->flush_on(logger->level());

    spdlog::set_default_logger(logger);

    if(prog.get<bool>("--tcp"))
    {
        logger->sinks().push_back(console_sink);
        if(prog.is_used("--log"))
            spdlog::default_logger()->sinks().push_back(file_sink);

        if(prog.get<bool>("--forward-log"))
            spdlog::default_logger()->sinks().push_back(fwd_sink);
        
        do
        {
            {
                in_port_t port = prog.get<in_port_t>("--port");
                sockpp::initialize();
                slsp::TCPInterfaceServer itf = slsp::TCPInterfaceServer(port);
                spdlog::info("Diplomat Language Server version {}",DIPLOMAT_VERSION_STRING);
                spdlog::info("Await client on port {}...",port);
                itf.await_client();
                
                std::istream tcp_input(&itf);
                std::ostream tcp_output(&itf);

                DiplomatLSP lsp(tcp_input,tcp_output);

                if(prog.get<bool>("--forward-log"))
                {
                    fwd_sink->set_target_lsp(&lsp);
                }
                
                runner(lsp);
            }
            
            if(prog.get<bool>("--allow-reboot"))
            {
                spdlog::info("Diplomat Language Server stopped in TCP mode is allowed to reboot...");
                spdlog::info("LSP will reboot, use Ctrl-C to exit.");
            }
            else
            {
                spdlog::info("Diplomat Language Server stopped in TCP but --allow-reboot was not provided.");
                spdlog::info("Shutting down, bye!");
                break;
            }
        } while (prog.get<bool>("--allow-reboot"));
    }
    else
    {
        // auto logger = std::make_shared<spdlog::logger>("main");
         // spdlog::basic_logger_mt("main","./diplomat-lsp.log");
        logger->sinks().push_back(file_sink);      
        logger->info("Start new log.");


        DiplomatLSP lsp = DiplomatLSP();
        lsp.set_rpc_use_endl(false);
        lsp.set_watch_client_pid(false);

        if(prog.get<bool>("--forward-log"))
        {
            if(spdlog::get_level() >= spdlog::level::trace)
                spdlog::set_level(spdlog::level::debug);
                
            fwd_sink->set_target_lsp(&lsp);
            spdlog::default_logger()->sinks().push_back(fwd_sink);
        }
        spdlog::info("Diplomat Language Server version {}",DIPLOMAT_VERSION_STRING);
        runner(lsp);
    }
    return 0;
}
