set (UTILS_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mdtime.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${UTILS_SOURCES})
target_sources(melodeer-exec PRIVATE ${UTILS_SOURCES})

