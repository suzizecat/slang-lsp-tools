set(LSP_METAMODEL_PATH ${CMAKE_CURRENT_BINARY_DIR}/metaModel.json)
set(LSP_ADDITIONAL_METAMODEL_PATHS ${CMAKE_SOURCE_DIR}/tools/json_schema/exchangeTypes.metaModel.json)

set(LSP_DATASTRUCT_ROOT ${CMAKE_SOURCE_DIR}/lsp-server/include/types)
set(LSP_DATASTRUCT_CMAKE ${LSP_DATASTRUCT_ROOT}/generated_headers.cmake)

if(NOT EXISTS ${LSP_DATASTRUCT_CMAKE} OR NOT EXISTS ${LSP_METAMODEL_PATH})
    
    message(STATUS "Regenerating LSP data structures using " ${LSP_METAMODEL_PATH})
    file(
        DOWNLOAD https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/metaModel/metaModel.json 
        ${LSP_METAMODEL_PATH}
        )

    message(STATUS "Command is python3 ${CMAKE_SOURCE_DIR}/tools/LSPGen/main.py -fo ${LSP_DATASTRUCT_ROOT} ${LSP_METAMODEL_PATH} ${LSP_ADDITIONAL_METAMODEL_PATHS}")
    execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/LSPGen/main.py -fo ${LSP_DATASTRUCT_ROOT} ${LSP_METAMODEL_PATH} ${LSP_ADDITIONAL_METAMODEL_PATHS})
    message(STATUS "Regeneration done. ")

else()
message(STATUS "LSP Datastructure already exists.")
message(STATUS "Command would have been python3 ${CMAKE_SOURCE_DIR}/tools/LSPGen/main.py -fo ${LSP_DATASTRUCT_ROOT} ${LSP_METAMODEL_PATH} ${LSP_ADDITIONAL_METAMODEL_PATHS}")
     
endif()

include(${LSP_DATASTRUCT_CMAKE})
list(TRANSFORM SVLS_GEN_HEADERS PREPEND ${LSP_DATASTRUCT_ROOT}/)