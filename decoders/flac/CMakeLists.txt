set(FLAC_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mdflac.c)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(FLAC REQUIRED)

target_sources(melodeer PRIVATE ${FLAC_SOURCES})
target_sources(melodeer-exec PRIVATE ${FLAC_SOURCES})

