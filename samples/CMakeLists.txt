# Interactive samples / tests

function (add_mg_sample name)
    add_executable (${name} ${name}.cpp)
    target_link_libraries (${name} mg_engine)
endfunction()

add_mg_sample(test_scene)

add_mg_sample(input)

