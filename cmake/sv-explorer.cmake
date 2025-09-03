
create_component(sv-explorer 
LIB_SRC 
    PRIVATE explorer/hier_visitor.cpp
LIB_INC
    INTERFACE explorer/
LIB_LINK
    PUBLIC slang::slang
    PUBLIC nlohmann_json::nlohmann_json
EXE_SRC
    PRIVATE explorer/main.cpp
    PRIVATE explorer/explorer_visitor.cpp
EXE_INC 
    PRIVATE utils/ast-printer
EXE_LINK
    PRIVATE slang::slang
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE argparse::argparse
BUILD_DEFINES
    PRIVATE DIPLOMAT_VERSION_STRING=\"${VERSION_STRING}\"
 )