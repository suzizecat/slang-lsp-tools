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

        /**
         * @brief Input stream
         * 
         */
        std::istream& _in;
        std::ostream& _out;

        std::condition_variable _data_available;

        std::jthread _inbox_manager;
        std::jthread _outbox_manager;

        std::stop_source _ss;

        bool _closed;
        bool _aborted;

        bool _use_endl;

        /**
         * @brief This function will retrieve data from the input
         * stream ::_in and transfer it (as json) to the #_inbox.
         * 
         * @note This function is expected to be used as the #_inbox_manager function.
         *
         * @param stok stop token, used to exit the process upon stopping the LS.
         *  \sa _get_json() is used to actually extract data.
         */
        void _poll_inbox(std::stop_token stok);

        /**
         * @brief This function wait for data to be in the #_outbox and then
         * transfer it through the medium provided by #_out .
         * 
         * @param stok Stop token to gracefully handle stop of operations.
         */
        void _push_outbox(std::stop_token stok);

        /**
         * @brief Low level function used to capture a json string from the 
         * input stream ::_in . 
         * 
         * @param stok Stop token, used to stop the capture gracefully as soon as possible
         * upon external stop request.
         * @return nlohmann::json with the extracted RPC call content on success, empty json otherwise.
         */
        nlohmann::json _get_json(std::stop_token& stok);

    public:
        RPCPipeTransport() = delete;
        RPCPipeTransport(std::istream &input, std::ostream &output);
        ~RPCPipeTransport();

        void send(const nlohmann::json& data);
        void abort();
        void close();
        inline void set_endl(const bool use_endl) {_use_endl = use_endl;};
        inline bool is_closed() const { return _closed || _aborted; };
        nlohmann::json get();
    };
};