#pragma once

#include <string>
#include <optional>

namespace slsp{
    namespace type
    {
        typedef struct clientInfo_t
        {
            /**
             * The name of the client as defined by the client.
             */
            std::string name;

            /**
             * The client's version as defined by the client.
             */
            std::optional<std::string> version;

        } clientInfo_t;
    }
}