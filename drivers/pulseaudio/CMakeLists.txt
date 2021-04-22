set(PAUDIODRV_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mdpaudio.c)

target_sources(melodeer PRIVATE ${PAUDIODRV_SOURCES})
target_sources(melodeer-exec PRIVATE ${PAUDIODRV_SOURCES})


