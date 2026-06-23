add_subdirectory(config)
add_subdirectory(wire)
add_subdirectory(data)

add_executable(rask
        main.cpp
    )
target_compile_features(rask PRIVATE cxx_std_23)
target_link_libraries(rask PRIVATE
        rask-data
        rask-wire
        rask-config
        planet
        felspar-io
    )
install(TARGETS rask RUNTIME DESTINATION bin)
