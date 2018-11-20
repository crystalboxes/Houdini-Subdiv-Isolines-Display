cmake_minimum_required( VERSION 3.6 )
project( HDK_Project )

list( APPEND CMAKE_PREFIX_PATH "$ENV{HOUDINI_HFS_DIR}/toolkit/cmake" )

find_package( Houdini REQUIRED )
set( library_name Isolines )

add_library( ${library_name} SHARED
  GeometryUtilities.h
  SOP_Isolines.h
  SOP_Isolines.cpp
  DM_Isolines.h
  DM_Isolines.cpp
  IsolineMaker.h
  IsolineMaker.cpp
)

target_link_libraries( ${library_name} Houdini )

target_include_directories( ${library_name} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

houdini_configure_target( ${library_name} )
