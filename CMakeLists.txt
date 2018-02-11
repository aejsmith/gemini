cmake_minimum_required(VERSION 3.5)
project(Orion)

set(default_build_type "Debug")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()

set(COMMON_FLAGS    "")
set(GENERATOR_FLAGS "")
set(C_FLAGS         "")
set(CXX_FLAGS       "")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    string(CONCAT COMMON_FLAGS
        "-Wall -Wextra -Wno-variadic-macros -Wno-unused-parameter "
        "-Wwrite-strings -Wmissing-declarations -Wredundant-decls "
        "-Wno-format -Wno-unused-function -Wno-comment "
        "-Wno-unused-private-field "
    )

    string(CONCAT CXX_FLAGS
        "-Wsign-promo"
    )

    # Running under Ninja we don't get colour diagnostics by default since it
    # buffers output.
    if (${CMAKE_GENERATOR} STREQUAL "Ninja")
        set(GENERATOR_FLAGS "-fcolor-diagnostics")
    endif ()

    set(DEBUG_FLAGS          "-Og -g")
    set(RELEASE_FLAGS        "-O3 -g -DNDEBUG")
else ()
    message(FATAL_ERROR "Unsupported compiler")
endif ()

set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} ${COMMON_FLAGS} ${GENERATOR_FLAGS} ${C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG     "${DEBUG_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE   "${RELEASE_FLAGS}")

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} ${GENERATOR_FLAGS} ${CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG   "${DEBUG_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${RELEASE_FLAGS}")
set(CMAKE_CXX_STANDARD      14)

add_executable(Test
    "Engine/Runtime/Engine/Source/Main.cpp"
)
