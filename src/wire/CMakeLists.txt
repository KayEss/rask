add_library(rask-wire STATIC
        hash.cpp
        identity.cpp
        tick.cpp
    )
target_compile_features(rask-wire PUBLIC cxx_std_23)
target_include_directories(rask-wire PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rask-wire PUBLIC planet)
