#include <iostream>
#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

#include "nlohmann/json.hpp"
#include "lsp.h"
#include "rpc_transport.hpp"

using json = nlohmann::json;

void runner()
{
    rpc::RPCPipeTransport itf(std::cin, std::cout);
    json data;
    do
    {
        data = itf.get();
        spdlog::info("Got data ! {}", data.dump());
        itf.send(data);
    } while (!data.contains("exit"));
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
