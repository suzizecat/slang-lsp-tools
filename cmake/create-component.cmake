function(create_component NAME)
    set(options NOLIB)
    set(multiValueArgs 
        LIB_SRC 
        LIB_INC
        LIB_LINK 
        EXE_SRC 
        EXE_INC 
        EXE_LINK
    )
    cmake_parse_arguments(PARSE_ARGV 1 COMP "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if(NOT ${COMP_NOLIB})
        add_library(${NAME}-lib STATIC)
        if(DEFINED COMP_LIB_SRC)
            target_sources(${NAME}-lib ${COMP_LIB_SRC})
        endif()

        if(DEFINED COMP_LIB_LINK)
            target_link_libraries(${NAME}-lib ${COMP_LIB_LINK})
        endif()

        if(DEFINED COMP_LIB_INC)
            target_include_directories(${NAME}-lib ${COMP_LIB_INC})
        endif()

        if(NOT DEFINED COMP_EXE_LINK)
            set(COMP_EXE_LINK "")
        endif()

        if(NOT DEFINED COMP_EXE_INC)
            set(COMP_EXE_INC "")
        endif()
        
        list(PREPEND COMP_EXE_LINK PRIVATE ${NAME}-lib)
        list(PREPEND COMP_EXE_INC PRIVATE $<TARGET_PROPERTY:${NAME}-lib,INTERFACE_INCLUDE_DIRECTORIES>)
        # message(STATUS "    EXE INC : ${COMP_EXE_INC}")
        # Override the output name to avoid libNAME-lib.so
        # The lib prefix is automatically added.
        set_property(TARGET ${NAME}-lib PROPERTY OUTPUT_NAME ${NAME})
    else()
        message(STATUS "No library for target component ${NAME}")
    endif()

    add_executable(${NAME}-exe)
    if(DEFINED COMP_EXE_SRC) 
        target_sources(${NAME}-exe ${COMP_EXE_SRC})
    else()
        message(ERROR "Component ${NAME} have no source specified for its executable (EXE_SRC)")
    endif()


    target_link_libraries(${NAME}-exe ${COMP_EXE_LINK})
    target_include_directories(${NAME}-exe ${COMP_EXE_INC})

    # Override the output name of the executable to have a clean NAME
    # instead of NAME-exe. 
    set_property(TARGET ${NAME}-exe PROPERTY OUTPUT_NAME ${NAME})

    message(STATUS "Created targets for component ${NAME}.")

endfunction(create_component)
