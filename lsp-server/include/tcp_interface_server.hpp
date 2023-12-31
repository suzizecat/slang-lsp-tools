#pragma once

#include <iostream>
#include <string>
#include <thread>
#include "sockpp/tcp_acceptor.h"

#include "spdlog/spdlog.h"

#include <array>
#include <sstream>

namespace slsp {
    class TCPInterfaceServer : public std::stringbuf
    {
        protected:
            std::string _listening_address;
            in_port_t _listening_port;
            sockpp::tcp_acceptor _acc;
            sockpp::tcp_socket _sock;
            std::array<char,512> _rx;
            // char _txbuf[512];

            std::streamsize xsputn(const char_type* s, std::streamsize n) override;
            // std::streamsize xsgetn(char_type* s, std::streamsize n) override;
            int underflow() override;



            int _read_data();
        public: 

            TCPInterfaceServer(in_port_t port, const std::string addr = "0.0.0.0");

            bool await_client();
            void send(const std::string& data);
            std::string receive();

            std::stringstream _out;
            const std::stringstream& get_out_stream() {return _out;};
    };

}