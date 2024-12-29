create_component(sv-indexer 
LIB_SRC 
    PRIVATE indexer/index_elements.cpp
    PRIVATE indexer/index_symbols.cpp
    PRIVATE indexer/index_file.cpp
    PRIVATE indexer/index_scope.cpp
    PRIVATE indexer/index_core.cpp
    PRIVATE indexer/index_visitor.cpp
    PRIVATE indexer/index_reference_visitor.cpp
LIB_INC
    PUBLIC indexer/include
LIB_LINK
    PUBLIC slang::slang
    PUBLIC nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
EXE_SRC
    PRIVATE indexer/main_indexer.cpp
 )