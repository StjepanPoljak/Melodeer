cmake_minimum_required (VERSION 2.6)
project (Melodeer C)

set (CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package (Threads REQUIRED)
find_package (FLAC REQUIRED)
find_package (OpenAL REQUIRED)
find_package (MPG123 REQUIRED)

set (Melodeer_VERSION_MAJOR 1)
set (Melodeer_VERSION_MINOR 0)

set (SRC_FILES
	source/main.c
	source/mdcore.c
	source/mdflac.c
	source/mdlog.c
	source/mdmpg123.c
	source/mdutils.c
	source/mdwav.c)

include_directories (
	"${PROJECT_SOURCE_DIR}/include"
)

add_executable (melodeer-exec source/main.c ${SRC_FILES})
add_library (melodeer SHARED ${SRC_FILES})

set (MD_LIBS
	${CMAKE_THREAD_LIBS_INIT}
	FLAC
	openal
	mpg123
	m)

target_link_libraries (melodeer ${MD_LIBS})
target_link_libraries (melodeer-exec ${MD_LIBS})

install(DIRECTORY DESTINATION include/melodeer)

install(TARGETS melodeer DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/melodeer)
