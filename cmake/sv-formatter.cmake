create_component(sv-formatter 
LIB_SRC 
    PRIVATE formatter/format_StatementExpansion.cpp
    PRIVATE formatter/format_DataDeclaration.cpp
    PRIVATE formatter/spacing_manager.cpp
LIB_INC
    INTERFACE formatter/
LIB_LINK
    PUBLIC slang::slang
EXE_SRC
    PRIVATE utils/ast-printer/ast_print.cpp
    PRIVATE formatter/main_formatter.cpp
EXE_INC 
    PRIVATE utils/ast-printer
EXE_LINK
    PRIVATE argparse::argparse
    PRIVATE fmt::fmt
    PRIVATE spdlog::spdlog
    PRIVATE slang::slang
    PRIVATE sv-formatter-lib
BUILD_DEFINES
    PRIVATE DIPLOMAT_VERSION_STRING=\"${VERSION_STRING}\"
 )