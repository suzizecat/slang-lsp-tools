#include "tcp_interface_server.hpp"
#include "spdlog/spdlog.h"

namespace slsp {
    std::streamsize TCPInterfaceServer::xsputn(const TCPInterfaceServer::char_type *s, std::streamsize n)
    {
        spdlog::trace("Trying to write {} bytes : {}",n,std::string(s,n));
        return _sock.write_n(s,n);
    }

    int TCPInterfaceServer::_read_data()
    {
        int n = 0;
        spdlog::debug("Read data from TCP");
        if(gptr() != egptr())
        {
            n = _sock.read(gptr(),_rx.end() - gptr());
            setg(eback(),gptr(),gptr() + n);
        }
        else
        {
            n = _sock.read(_rx.begin(),_rx.size());
            setg(_rx.begin(),_rx.begin(),_rx.begin()+n);
        }
        spdlog::debug("Read data from TCP done, {} bytes read.",n);
        spdlog::trace("{} bytes available in stream.",in_avail());
        return n;
    }

    int TCPInterfaceServer::underflow()
    {
        char buff[2];
        buff[1] = 0;
        int n = 0;
        n = _read_data();

        if (n > 0)
        {
            spdlog::debug("Underflow, read data {}",std::string(buff));
            return *gptr();
        }
        else
        {
            spdlog::debug("Got EOF");
            return traits_type::eof();
        }
    }

    TCPInterfaceServer::TCPInterfaceServer( in_port_t port, const std::string addr) :
        _listening_address(addr),
        _listening_port(port),
        _acc(port),
        _out()
    {    
        std::stringbuf::setg(_rx.begin(),_rx.begin(),_rx.begin());
    }

    TCPInterfaceServer::~TCPInterfaceServer()
    {
        spdlog::info("Destroying the TCP medium");
        flush();
        
        _sock.close();

    }

    bool TCPInterfaceServer::await_client()
    {
        sockpp::inet_address peer;
        _sock = _acc.accept(&peer);
        spdlog::info("Incoming connection from {}", peer.to_string());
        if(!_sock)
        {
            spdlog::error("Issue in accepting client {}", _acc.last_error_str());
            return false;
        }
        
        return true;
    }

    void TCPInterfaceServer::send(const std::string& data)
    {
        _sock.write(data);
    }

    void TCPInterfaceServer::flush()
    {
        spdlog::info("Flushing server communication interface.");
        while (_sock.read(_rx.begin(),_rx.size()) > 0) {/*nothing, discard*/}

        // Reset the 'get' positions to the begining.
        setg(_rx.begin(),_rx.begin(),_rx.begin());

        _sock.clear();
    }

}
