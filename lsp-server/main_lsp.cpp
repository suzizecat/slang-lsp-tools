#include <iostream>
#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

#include "nlohmann/json.hpp"
#include "lsp.hpp"
#include "rpc_transport.hpp"

using json = nlohmann::json;

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

void lsp_shutdown(slsp::BaseLSP* lsp, json& params)
{
    lsp->shutdown();
}

void runner()
{
    slsp::BaseLSP lsp;
    lsp.bind_notification("hello", say_hello);
    lsp.bind_notification("exit", lsp_shutdown);
    lsp.bind_request("add", adder);
    lsp.run();
    spdlog::info("Clean exit.");
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("slang Language server", "0.0.1");
    prog.add_argument("name").help("Give the name to say hello");
    try{
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }
    spdlog::info("Hello {} !", prog.get("name"));
    runner();
    return 0;
}
