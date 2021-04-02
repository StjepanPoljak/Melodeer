set (CORE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/main.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdcore.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdcoreops.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdsettings.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${CORE_SOURCES})
target_sources(melodeer-exec PRIVATE ${CORE_SOURCES})

add_subdirectory(buffer)

