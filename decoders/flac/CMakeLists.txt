list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package (FLAC REQUIRED)

target_sources(melodeer PRIVATE mdflac.c)
target_sources(melodeer-exec PRIVATE mdflac.c)

target_link_libraries(melodeer FLAC)
target_link_libraries(melodeer-exec FLAC)
