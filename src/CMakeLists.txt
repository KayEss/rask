add_executable(rask
        main.cpp
    )
target_compile_features(rask PRIVATE cxx_std_23)
target_link_libraries(rask PRIVATE
        planet
        felspar-io
    )
install(TARGETS rask RUNTIME DESTINATION bin)
