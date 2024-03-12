#pragma once

#include "nlohmann/json.hpp"
#include <thread>
#include <condition_variable>

#include <chrono>
#include <queue>
#include <istream>
#include <ostream>


namespace rpc
{
    class RPCPipeTransport
    {
    protected:
        std::queue<nlohmann::json> _inbox;
        std::queue<nlohmann::json> _outbox;

        std::mutex _rx_access;
        std::mutex _tx_access;

        std::istream& _in;
        std::ostream& _out;

        std::condition_variable _data_available;

        std::jthread _inbox_manager;
        std::jthread _outbox_manager;

        std::stop_source _ss;

        bool _closed;
        bool _aborted;

        bool _use_endl;

        void _poll_inbox(std::stop_token stok);
        void _push_outbox(std::stop_token stok);

        nlohmann::json _get_json();

    public:
        RPCPipeTransport() = delete;
        RPCPipeTransport(std::istream &input, std::ostream &output);
        ~RPCPipeTransport();

        void send(const nlohmann::json& data);
        void abort();
        inline void set_endl(const bool use_endl) {_use_endl = use_endl;};
        inline bool is_closed() const { return _closed || _aborted; };
        nlohmann::json get();
    };
};