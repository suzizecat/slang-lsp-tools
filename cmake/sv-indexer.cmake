create_component(sv-indexer 
LIB_SRC 
    PRIVATE indexer/index_elements.cpp
    PRIVATE indexer/diplomat_index.cpp
    PRIVATE indexer/visitor_index.cpp
LIB_INC
    PUBLIC indexer/include
LIB_LINK
    PUBLIC slang::slang
    PUBLIC nlohmann_json::nlohmann_json
    spdlog::spdlog
EXE_SRC
    PRIVATE indexer/main_indexer.cpp
 )