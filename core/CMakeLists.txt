set (CORE_SOURCES
	main.c
	mdcore.c
	mdcoreops.c
	mdsettings.c)

target_include_directories(melodeer PRIVATE include)
target_include_directories(melodeer-exec PRIVATE include)

target_sources(melodeer PRIVATE ${CORE_SOURCES})
target_sources(melodeer-exec PRIVATE ${CORE_SOURCES})

add_subdirectory(buffer)

