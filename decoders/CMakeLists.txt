set (DECODER_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mddecoder.c
	${CMAKE_CURRENT_SOURCE_DIR}/mddecoderll.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${DECODER_SOURCES})
target_sources(melodeer-exec PRIVATE ${DECODER_SOURCES})

add_subdirectory(flac)
add_subdirectory(dummy)

