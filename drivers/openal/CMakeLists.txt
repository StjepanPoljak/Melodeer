set(OPENAL_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/mdopenal.c)

find_package(OpenAL)

IF(OpenAL_FOUND)
	target_sources(melodeer PRIVATE ${OPENAL_SOURCES})
	target_sources(melodeer-exec PRIVATE ${OPENAL_SOURCES})
ENDIF()
