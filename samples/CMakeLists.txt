# Interactive samples / tests

function (add_mg_sample name)
    add_executable (${name} ${name}.cpp)

    # Set rpath for easier deployment:
    # Place shared libs in ../libs directory relative to executable.
    set_target_properties (${name} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/../libs"
        DEBUG_POSTFIX _d
    )

    target_link_libraries (${name} mg_engine)
endfunction()

add_mg_sample(test_scene)

add_mg_sample(input)

