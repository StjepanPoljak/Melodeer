cmake_minimum_required (VERSION 3.11)
project (Melodeer C)

#set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
#find_package(MPG123 REQUIRED)

set(Melodeer_VERSION_MAJOR 2)
set(Melodeer_VERSION_MINOR 0)

add_executable(melodeer-exec)
add_library(melodeer SHARED)

add_definitions(-DCONFIG_DEBUG -g)

set (MD_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	openal
	FLAC
	mpg123
	m) # m is for math

target_include_directories(melodeer PUBLIC include)
target_include_directories(melodeer-exec PUBLIC include)

target_link_libraries(melodeer ${MD_LIBS})
target_link_libraries(melodeer-exec ${MD_LIBS})

install(DIRECTORY DESTINATION include/melodeer)

install(TARGETS melodeer DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/melodeer)

add_subdirectory(core)
add_subdirectory(decoders)
add_subdirectory(drivers)
