set(DUMMYDRV_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mddummydrv.c)

target_sources(melodeer PRIVATE ${DUMMYDRV_SOURCES})
target_sources(melodeer-exec PRIVATE ${DUMMYDRV_SOURCES})


