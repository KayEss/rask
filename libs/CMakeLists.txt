# Vendored dependencies, pulled in as git submodules.
#
# planet normally fetches the Felspar libraries with FetchContent (see its
# requirements.cmake). We provide them ourselves as submodules instead, so they
# must be added in dependency order — each library's own requirements.cmake is
# skipped when it is built as a subdirectory rather than as the top-level
# project, and it expects the targets it links against to already exist.
#
#   test        -> (none)
#   exceptions  -> test
#   memory      -> test, exceptions
#   coro        -> test, exceptions, memory
#   parse       -> test, exceptions, memory, coro
#   io          -> test, exceptions, memory, coro
#   planet      -> io, parse (+ test, coro, exceptions, memory)
#
# BLAKE3 is independent of the felspar libraries; its CMake build lives in c/.
add_subdirectory(BLAKE3/c)
add_subdirectory(felspar-test)
add_subdirectory(felspar-exceptions)
add_subdirectory(felspar-memory)
add_subdirectory(felspar-coro)
target_compile_definitions(felspar-coro INTERFACE FELSPAR_CORO_SKIP_LIFETIME_CHECKS)
add_subdirectory(felspar-parse)
add_subdirectory(felspar-io)
add_subdirectory(planet)
