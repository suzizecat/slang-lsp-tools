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

namespace slsp {

    /**
     * @brief Base class for RPC exception (and, by extension any LSP exception)
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
    };

    // No need for from_json as we can't really update an exception.

    /**
     * @brief Invalid JSON recieved error
     */
    MAKE_BASIC_RPC_EXCEPTION(rpc_parse_error, types::ErrorCodes_ParseError);
    // class rpc_parse_error : public rpc_base_exception
    // {
    //     public:
    //     rpc_parse_error(const std::string msg) : rpc_base_exception(types::ErrorCodes_ParseError, msg) {};
    //     rpc_parse_error(const std::string msg, const nlohmann::json& data) : rpc_base_exception(types::ErrorCodes_ParseError, msg,data) {};
    // };

    /**
     * @brief The recieved JSON is valid, but does not represent a valid RPC object
     */
    MAKE_BASIC_RPC_EXCEPTION(rpc_invalid_request_error, types::ErrorCodes_InvalidRequest);
    // class rpc_invalid_request_error : public rpc_base_exception
    // {
    //     public:
    //     rpc_invalid_request_error(const std::string msg) : rpc_base_exception(types::ErrorCodes_ParseError, msg) {};
    //     rpc_invalid_request_error(const std::string msg, const nlohmann::json& data) : rpc_base_exception(types::ErrorCodes_ParseError, msg,data) {};
    // };
    
    

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

    void to_json(nlohmann::json& j, const rpc_base_exception& e);
   

}