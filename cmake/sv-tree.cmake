create_component(sv-tree 
LIB_SRC 
    PRIVATE utils/ast-printer/ast_print.cpp
LIB_INC
    PUBLIC utils/ast-printer
LIB_LINK
    PUBLIC slang::slang
    PRIVATE argparse::argparse
EXE_SRC
    PRIVATE utils/ast-printer/ast_print_main.cpp
 )