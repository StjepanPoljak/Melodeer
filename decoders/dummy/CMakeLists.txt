set(DUMMYDEC_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mddummydec.c)

target_sources(melodeer PRIVATE ${DUMMYDEC_SOURCES})
target_sources(melodeer-exec PRIVATE ${DUMMYDEC_SOURCES})


