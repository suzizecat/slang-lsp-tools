#include "rpc_transport.hpp"
#include <future>
#include <chrono>
#include <string>
#include "spdlog/spdlog.h"
#include "fmt/format.h"
#include "lsp_errors.hpp"
using json = nlohmann::json;

using namespace std::chrono_literals;

namespace rpc {
RPCPipeTransport::RPCPipeTransport(std::istream& input, std::ostream& output) :
    _inbox(),
    _outbox(),
    _rx_access(),
    _tx_access(),
    _in(input),
    _out(output),
    _data_available(),
    _ss(),
    _closed(false)
    {
        _inbox_manager = std::jthread(&RPCPipeTransport::_poll_inbox, this, _ss.get_token());
        _outbox_manager = std::jthread(&RPCPipeTransport::_push_outbox, this, _ss.get_token());
    }

    RPCPipeTransport::~RPCPipeTransport()
    {
        _ss.request_stop();

        if(_outbox_manager.joinable())
            _outbox_manager.join();
    
        
        if(_inbox_manager.joinable())
            _inbox_manager.join();
        
        spdlog::info("Pipe transport destructed");
    }


    json RPCPipeTransport::_get_json()
    {
        json ret;
        std::string rx_header;
        bool in_header = true;
        bool got_header = false; 
        bool data_valid = false;
        do
        {   
            do
            {
                std::getline(_in,rx_header);
                if(rx_header.length() > 1)
                {
                    got_header = true;
                }
                else if(got_header)
                {
                    got_header = false;
                    in_header = false;
                }
            }while(in_header);
            
            try
            {
                in_header = true;
                _in >> ret;
                data_valid = true;
            }
            catch (nlohmann::json::parse_error e)
            {
                if(_in.eof())
                {
                    spdlog::warn("Catched EOF");
                    _in.ignore(std::numeric_limits<std::streamsize>::max());
                    ret = R"({
                        	"jsonrpc": "2.0",
                            "id": 0,
                            "method": "exit",
                            "params": null
                            })"_json;
                    return ret;
                }
                else
                {
                    std::this_thread::sleep_for(100ms);
                    spdlog::error("Ignored ill-formed JSON input : {}", std::string(e.what()));
                    ret.clear();
                    char* buf = new char(_in.rdbuf()->in_avail());
                    _in.readsome(buf, _in.rdbuf()->in_avail());
                    delete buf;
                    //_in.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
                }
            }
        } while (!data_valid);

        return ret;
    }

    void RPCPipeTransport::_poll_inbox(std::stop_token stok)
    {
        using namespace std::chrono_literals;

        std::packaged_task<json()> task;
        std::future<json> future_line;
        std::thread t;

        //future_line = std::async(std::launch::async, &RPCPipeTransport::_get_json, this);
        task = std::packaged_task<json()>([this](){ return this->_get_json(); });
        future_line = task.get_future();
        t = std::thread((std::move(task)));

        while (!stok.stop_requested() && !_in.eof())
        {
            //spdlog::info("Check {}", stok.stop_requested());
            std::future_status status;
            if(status = future_line.wait_for(100ms); status == std::future_status::ready )
            {
                json data = future_line.get();
                t.join();
                spdlog::debug("Captured data {}", data.dump());

                {
                    std::lock_guard<std::mutex> lock(_rx_access);
                    _inbox.push(data);
                    data = json();
                }
                _data_available.notify_all();
                if(! stok.stop_requested())
                {
                    task = std::packaged_task<json()>([this](){ return this->_get_json(); });
                    future_line = task.get_future();
                    t = std::thread((std::move(task)));
                }
            }
        }
        t.detach();
        _closed = true;
        _data_available.notify_all();
        spdlog::info("Stop polling inbox.");
    }

    void RPCPipeTransport::_push_outbox(std::stop_token stok)
    {
        using namespace std::chrono_literals;
        bool remaining_data;
        while (!stok.stop_requested())
        {
            remaining_data = false;
            {
                std::lock_guard<std::mutex> lock(_tx_access);
                if(! _outbox.empty())
                {
                    std::string to_out = _outbox.front().dump();
                   
                    _out << fmt::format(
                        "Content-Length: {}\r\n"
                        "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n"
                        "\r\n"
                        "{}"
                        ,to_out.length(),to_out);

                    _outbox.pop();
                    remaining_data = _outbox.empty();
                }
            }

            std::this_thread::sleep_for(remaining_data ? 10ms : 100ms);
        }
        spdlog::info("Stop polling outbox.");
    }

    void RPCPipeTransport::send(const json& data)
    {
         
        json to_send;
        to_send["jsonrpc"] = "2.0";
        to_send.update(data);

        {
            std::lock_guard<std::mutex> lock(_tx_access);
            _outbox.push(to_send);
        }
    }

    json RPCPipeTransport::get()
    {
        {
            std::lock_guard<std::mutex> lock(_rx_access);
            if(!_inbox.empty())
            {
                json ret = _inbox.front();
                _inbox.pop();
                return ret;
            }
        }

        std::unique_lock lk(_rx_access);
        spdlog::debug("Await for incomming data...");
        _data_available.wait(lk, [this]
                             { return !this->_inbox.empty() || this->is_closed(); });
        if (_closed)
        {
            throw slsp::client_closed_exception("The client disconnected");
        }
        
        json ret = _inbox.front();
        _inbox.pop();
        spdlog::debug("Got valid data {}", ret.dump());
        lk.unlock();
        return ret;
    }
}