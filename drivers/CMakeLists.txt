set (DRIVER_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mddriver.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${DRIVER_SOURCES})
target_sources(melodeer-exec PRIVATE ${DRIVER_SOURCES})

add_subdirectory(openal)
add_subdirectory(dummy)
add_subdirectory(pulseaudio)
