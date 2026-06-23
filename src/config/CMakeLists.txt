add_library(rask-config STATIC
        identity.cpp
        peers.cpp
        subscriptions.cpp
    )
target_compile_features(rask-config PUBLIC cxx_std_23)
target_include_directories(rask-config PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rask-config PUBLIC planet)
