set (BUFFER_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mdbufchunk.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdbufll.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdevq.c
	${CMAKE_CURRENT_SOURCE_DIR}/mdgarbage.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${BUFFER_SOURCES})
target_sources(melodeer-exec PRIVATE ${BUFFER_SOURCES})

