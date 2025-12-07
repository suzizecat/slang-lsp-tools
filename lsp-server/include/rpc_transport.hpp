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
        /** Incoming messages captured, to be processed */
        std::queue<nlohmann::json> _inbox;
        /** Outcoming messages, to be sent */
        std::queue<nlohmann::json> _outbox;

        /** Ressource lock on the receiving interface (client to server) */
        std::mutex _rx_access;
        /** Ressource lock on the transmitting interface (server to client) */
        std::mutex _tx_access;

        /** Generic input stream */
        std::istream& _in;
        /** Generic output stream */
        std::ostream& _out;

        /** Trigger for waiting threads on the RX queue */
        std::condition_variable _data_available;

        /** Joinable thread for the incoming messages.
        * 
        * This thread interfaces with the RX stream, ensure that data
        * is properly received and push it to the #_inbox. It actually runs #_poll_inbox()
        */
        std::jthread _inbox_manager;
        /** Joinable thread for the outcoming messages
        *
        * This threads takes the next element of the #_outbox and sends it through #_out
        * using #_push_outbox()
        */
        std::jthread _outbox_manager;

        /** Source managing the graceful interruption of the whole medium. */
        std::stop_source _ss;

        /** Status flag of the medium used when the client closes the connection*/
        bool _closed;
        /** Status flag of the medium used when the server closes the connection*/
        bool _aborted;
        
        /** If set, adds an aditionnal endl after each sent messages*/
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

        /**
         * @brief Destroy the RPCPipeTransport object
         * 
         * Also implicitely handle stopping the I/O threads
         */
        ~RPCPipeTransport();

        /**
         * @brief Sends a RPC call payload through the medium.
         *
         * @note The RPC version field is automatically added.
         * 
         * @param data is a JSON object containing the relevant part of the JSON-RPC call.
         * expected object is: 
         * \code {.json}
         * {
         *      "method" : "methodName",
         *      "params" : { 
         *           param1 : {}
         *       }
         * }
         * \endcode
         */

        void send(const nlohmann::json& data);

        /**
         * @brief Wait for a valid JSON input and return it.
         *
         * @warning The compatibility with JSON-RPC is not checked.
         * 
         * @return nlohmann::json the full and row JSON object received.
         *
         * \sa #_get_json() for the input processing details
         */
        nlohmann::json get();

        /**
         * @brief Close the medium at the server initiative. 
         * Ultimately make the RX handler throw and destroy the RPC transport, thus stopping the I/O threads.
         */
        void abort();

        /**
         * @brief Close the medium at the client initiative. 
         * Ultimately make the RX handler throw and destroy the RPC transport, thus stopping the I/O threads.
         */
        void close();

        /** Set the #_use_endl usage property object */
        inline void set_endl(const bool use_endl) {_use_endl = use_endl;};
        /** Return the closed (#_closed or #_aborted) status of the medium */
        inline bool is_closed() const { return _closed || _aborted; };
        /** Have available data */
        inline bool have_data() const {return ! _inbox.empty(); };
    };
};
