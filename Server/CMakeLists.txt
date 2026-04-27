cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Server)

file(GLOB_RECURSE SERVER_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Server/*.cppm")
file(GLOB_RECURSE SERVER_IMPLS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Server/*.cpp")

add_executable(Server)

target_sources(Server
	PUBLIC
		FILE_SET cxx_modules TYPE CXX_MODULES
		BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Server"
		FILES ${SERVER_MODULES}
)

if(SERVER_IMPLS)
	target_sources(Server PRIVATE ${SERVER_IMPLS})
endif()

target_link_libraries(Server PRIVATE Engine)
target_link_libraries(Server PRIVATE GoonSquadLib)
