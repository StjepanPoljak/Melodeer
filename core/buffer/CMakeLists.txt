set (BUFFER_SOURCES
	mdbufchunk.c
	mdbufll.c
	mdgarbage.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${BUFFER_SOURCES})
target_sources(melodeer-exec PRIVATE ${BUFFER_SOURCES})

