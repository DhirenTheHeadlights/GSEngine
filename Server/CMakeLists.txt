cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Server)

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Server/Source/*.cppm")
file(GLOB_RECURSE SERVER_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Server/Include/*.cppm")

add_executable(Server ${SERVER_SOURCES})

target_sources(Server
	PUBLIC
		FILE_SET cxx_modules TYPE CXX_MODULES
		BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Server/Include"
		FILES ${SERVER_MODULES}
)

target_link_libraries(Server PRIVATE Engine)
target_link_libraries(Server PRIVATE GoonSquadLib)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/arch:AVX2) # SIMD optimizations
    add_compile_options(/MP)      # Multi-core compilation
endif()