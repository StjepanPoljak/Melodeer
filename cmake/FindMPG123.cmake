find_path (MPG123_INCLUDE_DIR mpg123.h)

find_library (MPG123_LIBRARY NAMES mpg123)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (MPG123 DEFAULT_MSG MPG123_LIBRARY MPG123_INCLUDE_DIR)

mark_as_advanced (MPG123_INCLUDE_DIR MPG123_LIBRARY)
