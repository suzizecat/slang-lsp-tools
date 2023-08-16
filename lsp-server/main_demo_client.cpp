#include <iostream>
#include "spdlog/spdlog.h"
#include "argparse/argparse.hpp"
#include "tcp_interface_server.hpp"
#include "sockpp/tcp_connector.h"
#include <thread>
#include "nlohmann/json.hpp"

int server(in_port_t port, bool should_reply)
{
    slsp::TCPInterfaceServer itf = slsp::TCPInterfaceServer(port);
    spdlog::info("Await client on port {}...",port);
    bool in_header = true;
    bool got_header = false;

    if(!itf.await_client())
        return 1;

    nlohmann::json recieved;
    std::string recieved_header;

    std::istream tcp_input(&itf);
    std::ostream tcp_output(&itf);

    int n = 0;
    char buf[2];
    buf[1] = 0;
    //tcp_input.unsetf(std::ios_base::skipws);
    while(true)
    {
        
        if(in_header)
        {
            std::getline(tcp_input,recieved_header);
            if(recieved_header.length() > 1)
            {
                spdlog::info("Header {}",recieved_header);
                got_header = true;
            }
            else if (got_header)
            {
                got_header = false;
                in_header = false;
            }
        }
        else
        {
            n++;
            
            do
            {
                recieved.clear();
                tcp_input >> recieved;
            } while (recieved.is_null());
            
            
            spdlog::info("Data {}",recieved.dump());

            if(!in_header && should_reply)
            {
                spdlog::debug("Echoing back {}",recieved.dump());
                tcp_output << recieved.dump();
            }

            if(recieved == "exit")
                break;
            
            in_header = true;
        }
        

        spdlog::debug("{} bytes remaining in stream.",itf.in_avail());

        if(! tcp_input.good())
        {
            spdlog::error("Broken output stream");
            spdlog::error("   EOF  :{}",itf._out.eof());
            spdlog::error("   fail :{}",itf._out.fail());
            spdlog::error("   bad  :{}",itf._out.bad());

            break;
        }
        else
        {
          
            if(n > 10)
                return 0;
        }
    }
    return 0;

}


int client(in_port_t port, bool expect_reply)
{
    using namespace std::chrono_literals;
    spdlog::info("Trying to connect to localhost:{} (5s...)",port);
    sockpp::tcp_connector conn({"localhost", port}, 5s);

    if(!conn)
    {
        spdlog::error("Did not manage to connect in time: {}",conn.last_error_str());
        return 1;
    }

    spdlog::info("Connected.");

    std::string  to_send_base;
    std::string  to_send;
    nlohmann::json input_data;
    std::string  received;

    std::ostringstream ss_send;
    char buffer[512] = "";

    while(! std::cin.eof())
    {
        ss_send.str("");
        spdlog::info("Await input data (Ctrl+D to exit)");
        std::cin >> input_data;

        if(std::cin.eof())
            break;

        to_send_base = input_data.dump();
        ss_send << "Content-Length: " << to_send_base.length() << "\r\n"
                << "\r\n"
                << to_send_base << "\r\n";

        to_send = ss_send.str();

        if(conn.write(to_send.c_str()) != ssize_t(to_send.length()))
        {
            spdlog::error("Error writing to TCP stream: {}",conn.last_error_str());
            return 1;
        }

        spdlog::info("Data sent: {}",to_send);

        if(expect_reply)
        {
            spdlog::info("Await any kind of feedback...");

            received.clear();
            memset(buffer,0,sizeof(buffer));
            int n = 0;
            do
            {
                n = conn.read(buffer,sizeof(buffer));
                received += std::string(buffer,n);
            } while (n == sizeof(buffer));

            spdlog::info("Received {}",received);
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    argparse::ArgumentParser prog("Demo TCP data sender", "0.0.1");
    prog.add_argument("--port","-p")
        .help("Port to use")
        .default_value<in_port_t>(8080)
        .scan<'u', in_port_t>();

    prog.add_argument("--client")
        .help("If provided, acts as a client")
        .default_value(false)
        .implicit_value(true);

    prog.add_argument("--bidir")
        .help("Use if you expect a reply")
        .default_value(false)
        .implicit_value(true);


    try {
        prog.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        spdlog::error(err.what());
        std::cerr << prog << std::endl;
        std::exit(1);
    }

    sockpp::initialize();
    in_port_t port = prog.get<in_port_t>("--port");
    bool bidir = prog.get<bool>("--bidir");

    if(prog.get<bool>("--client"))
        return client(port,bidir);
    else
        return server(port,bidir);
}
