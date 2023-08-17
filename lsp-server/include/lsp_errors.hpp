#pragma once

#include <exception>
#include <optional>
#include <string>
#include "types/enums/ErrorCodes.hpp"
#include "nlohmann/json.hpp"

#define MAKE_BASIC_RPC_EXCEPTION(typename, code) \
class typename : public rpc_base_exception{\
    public:\
    typename(const std::string msg) : rpc_base_exception(code, msg) {};\
    typename(const std::string msg, const nlohmann::json& data) : rpc_base_exception(code, msg,data) {};\
};


#define MAKE_BASIC_RPC_EXCEPTION_WITH_MSG(typename, code, msg) \
class typename : public rpc_base_exception{\
    public:\
    typename() : rpc_base_exception(code, msg) {};\
    typename(const nlohmann::json& data) : rpc_base_exception(code, msg,data) {};\
};

#define MAKE_BASIC_SRV_EXCEPTION(typename) \
class typename : public server_side_base_exception{\
    public:\
    typename(const std::string msg) : server_side_base_exception(msg) {};\
};

namespace slsp {

    /**
     * @brief Base class for RPC exception (and, by extension any LSP exception)
     *
     * Those exception may be converted to RPC message and sent back to the client.
     */
    class rpc_base_exception : public std::exception
    {
        protected:

        const std::string _message;
        const int _code;
        const std::optional<const nlohmann::json> _data;

        public:
        rpc_base_exception(const int code, const std::string msg) : _code(code), _message(msg) {};
        rpc_base_exception(const int code, const std::string msg, const nlohmann::json& data) : _code(code), _message(msg), _data(data) {};

        virtual const char* what() const noexcept { return _message.c_str(); };
        const std::string msg() const { return _message; };
        constexpr int code() const { return _code; };
        std::optional<nlohmann::json> data() const { return _data; };
        // No need for from_json as we can't really update an exception.
    };

    class server_side_base_exception : public std::runtime_error
    {
        public:
        server_side_base_exception(const std::string msg) : std::runtime_error(msg) {};
    };

    MAKE_BASIC_SRV_EXCEPTION(client_closed_exception);

    /**
     * @brief Invalid JSON recieved error
     */
    MAKE_BASIC_RPC_EXCEPTION(rpc_parse_error, types::ErrorCodes_ParseError);

    /**
     * @brief The recieved JSON is valid, but does not represent a valid RPC object
     */
    MAKE_BASIC_RPC_EXCEPTION(rpc_invalid_request_error, types::ErrorCodes_InvalidRequest);
    
    /**
     * @brief The requested method does not exists or is not available.
     */
    class rpc_method_not_found_error : public rpc_base_exception
    {
        public:
        rpc_method_not_found_error(const std::string method) :
            rpc_base_exception(types::ErrorCodes_MethodNotFound, "Method not found : " + method) {};
        rpc_method_not_found_error(const std::string method, const nlohmann::json& data) :
            rpc_base_exception(types::ErrorCodes_MethodNotFound, "Method not found : " + method, data) {};
    };

    /**
     * @brief The server recieved a request for a method while it is not initialized.
     * 
     */
    MAKE_BASIC_RPC_EXCEPTION_WITH_MSG(lsp_server_not_initialized_error,types::ErrorCodes_ServerNotInitialized,"Method call rejected due to uninitialized server.")


    void to_json(nlohmann::json& j, const rpc_base_exception& e);
   

}