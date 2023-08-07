#include "rpc_transport.hpp"
#include <future>
#include <chrono>
#include "spdlog/spdlog.h"
using json = nlohmann::json;

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
        spdlog::info("Pipe transport desctructor call.");
        _ss.request_stop();

        if(_outbox_manager.joinable())
            _outbox_manager.join();
        
        spdlog::info("Outbox joined");
        
        if(_inbox_manager.joinable())
            _inbox_manager.join();
        spdlog::info("Inbox joined");
        
        spdlog::info("Pipe transport desctructor done.");
    }

    std::string RPCPipeTransport::_get_line()
    {
        std::string ret; 
        std::getline(_in, ret);
        spdlog::info("_get_line {}", ret);
        return ret;
    }

    json RPCPipeTransport::_get_json()
    {
        json ret;
        try{
        _in >> ret;
        }
        catch (nlohmann::json::parse_error e)
        {
            if(_in.eof())
            {
                ret = json();
                ret["exit"] = 0;
                return ret;
            }
            else
            {
                throw e;
            }
        }

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

        while (!stok.stop_requested())
        {
            //spdlog::info("Check {}", stok.stop_requested());
            std::future_status status;
            if(status = future_line.wait_for(100ms); status == std::future_status::ready )
            {
                spdlog::info("Got something to handle !");
                json data = future_line.get();
                t.join();
                spdlog::info("Captured data {}", data.dump());

                {
                    std::lock_guard<std::mutex> lock(_rx_access);
                    _inbox.push(data);
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
                    _out << _outbox.front() << std::endl;
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
        spdlog::info("Await for incomming data...");
        _data_available.wait(lk, [this]
                             { return !this->_inbox.empty(); });

        json ret = _inbox.front();
        _inbox.pop();
        spdlog::info("Got valid data {}", ret.dump());
        lk.unlock();
        return ret;
    }
}