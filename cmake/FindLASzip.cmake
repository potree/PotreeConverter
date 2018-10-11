# - Locate LASzip library
# Defines:
#
#  LASzip_FOUND
#  LASzip_INCLUDE_DIR
#  LASzip_INCLUDE_DIRS (not cached)
#  LASzip_LIBRARY
#  LASzip_LIBRARIES (not cached)
#  LASzip_LIBRARY_DIRS (not cached)

# Look for LASzip in /opt/ (default install location using requirements.sh script)
find_path(LASzip_INCLUDE_DIR NAMES laszip_api.h HINTS /opt/LAStools/LASzip/dll/)
find_library(LASzip_LIBRARY NAMES laszip HINTS /usr/local/lib/)

# handle the QUIETLY and REQUIRED arguments and set LASzip_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LASzip DEFAULT_MSG LASzip_INCLUDE_DIR LASzip_LIBRARY)

mark_as_advanced(LASzip_FOUND LASzip_INCLUDE_DIR LASzip_LIBRARY)

set(LASzip_INCLUDE_DIRS ${LASzip_INCLUDE_DIR})
set(LASzip_LIBRARIES ${LASzip_LIBRARY})
get_filename_component(LASzip_LIBRARY_DIRS ${LASzip_LIBRARY} PATH)

