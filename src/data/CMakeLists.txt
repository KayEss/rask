add_library(rask-data STATIC
        filetype.cpp
        inode.cpp
        name_hash.cpp
        peers.cpp
        server.cpp
        subscriptions.cpp
        tenants.cpp
    )
target_compile_features(rask-data PUBLIC cxx_std_23)
target_include_directories(rask-data PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rask-data PUBLIC rask-wire planet)
