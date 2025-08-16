#include "rpc_transport.hpp"
#include <future>
#include <chrono>
#include <istream>
#include <string>
#include <thread>
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
    _closed(false),
    _aborted(false),
    _use_endl(true)
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


    /**
     * This function will wait for the proper start of the payload
     * of a RPC call (new line then '`{`' character ) while discarding
     * other data.
     * It will then capture all characters until the first brace is closed, denoting
     * the end of the JSON segment.
     *
     * This JSON is then parsed and returned.
     * Upon failure, the function will try to flush the input stream _in to avoid potential
     * corrupted data. Upon resuming, the function will not capture anything before having a proper
     * start of JSON.
     *
     * @note It is required for this function to handle each characters in order to be 
     * as non-blocking as possible to handle \p stok .
     *  
     */
    json RPCPipeTransport::_get_json(std::stop_token& stok)
    {
        std::string buf;
        using namespace std::chrono_literals;

        int json_idx = 0;
        bool in_string = false; 

        char last_char = 0;
        char new_char = 0;

        while (! _in.eof() && ! stok.stop_requested()) {
            // If "something" is waiting to be read
            if(_in.peek() != EOF)
            {
                last_char = new_char;
                new_char = _in.get();

                buf.push_back(new_char);
                
                // Skip everything as long as we don't have at least one
                // level of json ( '{' )
                if(json_idx == 0)
                {
                    if(! in_string && last_char == '\n' && new_char == '{')
                    {
                        spdlog::trace("Skipped data before json :\n{}",buf);
                        buf.clear();
                        json_idx ++;
                        // As we cleared the buffer, re-insert the brace.
                        buf.push_back(new_char);
                    }
                }
                else 
                {
                    if(new_char == '"' && last_char != '\\')
                            in_string = !in_string;
                    else if(! in_string)
                    {
                        if(new_char == '{')
                            json_idx ++;
                        else if(new_char == '}')
                            json_idx --;
                    }
                    
                    if(json_idx == 0)
                    {
                        // Parse the received data
                        try 
                        {
                            return json::parse(buf);
                        } 
                        catch (nlohmann::json::parse_error e) 
                        {
                            spdlog::error("Ignored ill-formed JSON input : {}\nBuffer was {}", std::string(e.what()),buf);
                            // Cleanup                            
                            while(_in.rdbuf()->in_avail())
                                _in.get();

                            return json();
                        }
                    }
                }
            }
            else 
            {
                std::this_thread::sleep_for(100ms);
            }
        }

        if(! stok.stop_requested() && _in.eof())
        {
        spdlog::warn("Caught EOF");
        _in.ignore(std::numeric_limits<std::streamsize>::max());
        return R"({
                "jsonrpc": "2.0",
                "id": 0,
                "method": "exit",
                "params": null
                })"_json;
        }

        // Fallback, hever would be the case where stok was triggered, so we don't care
        // about the value, we want to get out.
        return json();
    }


    void RPCPipeTransport::_poll_inbox(std::stop_token stok)
    {
         while (!stok.stop_requested() && !_in.eof())
        {
            json new_message = _get_json(stok);
            spdlog::trace("Captured data {}", new_message.dump(1));
            if(! new_message.empty())
            {
                // Push new json
                {
                    std::lock_guard<std::mutex> lock(_rx_access);
                    _inbox.push(new_message);
                }

                _data_available.notify_all();
            }
        }

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
                    to_out = fmt::format(
                        "Content-Length: {}\r\n"
                        "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n"
                        "\r\n"
                        "{}"
                        ,to_out.length(),to_out);
                    
                    _out << to_out;

                    if(_use_endl)
                        _out << std::endl;

                    _out.flush();

                    _outbox.pop();
                    remaining_data = _outbox.empty();
                }
            }

            std::this_thread::sleep_for(remaining_data ? 10ms : 100ms);
        }
        spdlog::info("Stop polling outbox.");
    }

    void RPCPipeTransport::abort()
    {
        spdlog::warn("Closing the RPC medium through 'abort' call.");
        _aborted = true;
        _data_available.notify_all();
    }

    void RPCPipeTransport::close()
    {
        spdlog::info("Closing the RPC medium through 'close' call.");
        _closed = true;
        _data_available.notify_all();
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
        if (is_closed())
        {
            throw slsp::server_connection_exception("The server tried to get data from a closed RPC medium.");
        }

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
        else if (_aborted)
        {
            throw slsp::server_connection_exception("The server closed the connection");
        }

        json ret = _inbox.front();
        _inbox.pop();
        spdlog::trace("Got valid data {}", ret.dump());
        lk.unlock();
         return ret;   
        
    }
}
