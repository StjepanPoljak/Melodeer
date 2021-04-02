find_package (OpenAL REQUIRED)

target_sources(melodeer PRIVATE mdopenal.c)
target_sources(melodeer-exec PRIVATE mdopenal.c)

target_link_libraries(melodeer openal)
target_link_libraries(melodeer-exec openal)
