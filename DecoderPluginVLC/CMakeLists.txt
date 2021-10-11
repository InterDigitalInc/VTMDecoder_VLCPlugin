project(VVCdecoder_plugin)
# minimum required cmake version
cmake_minimum_required( VERSION 3.5 FATAL_ERROR )

# executable
IF(WIN32)
set( LIB_NAME libvvcdecoder_plugin )
else()
set( LIB_NAME vvcdecoder_plugin )
endif()

# set default CMAKE_BUILD_TYPE to Release if not set
if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release." FORCE )
endif()

if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    set( USE_ADDRESS_SANITIZER OFF CACHE BOOL "Compiles with -sanitize=address and links to libasan" )
  endif()
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  set( BUILD_STATIC OFF CACHE BOOL "Build static executables" )
endif()

# set c++11
set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

# compile everything position independent (even static libraries)
set( CMAKE_POSITION_INDEPENDENT_CODE TRUE )

# enable sse4.1 build for all source files for gcc and clang
if( UNIX OR MINGW )
  add_compile_options( "-msse4.1" )
endif()

# enable parallel build for Visual Studio
if( MSVC )
  add_compile_options( "/MP" )
  add_compile_options( "/EHsc" )
endif()

# set address sanitizer compiler arguments
if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
  if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    if( USE_ADDRESS_SANITIZER )
      # add compile options
      add_compile_options( "-fsanitize=address" )
      add_link_options( "-fsanitize=address" )
    endif()
  endif()

  if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0 )
    add_compile_options( "-fabi-version=6" )
  endif()
endif()

# get source files
file( GLOB SRC_FILES "*.cpp" )

set( VTM_DIR "VVCSoftware_VTM/" CACHE PATH "path to VTM lib include directory and import lib for windows" )

set( VLC_PROGRAM_DIR "C:/VLC" CACHE PATH "will output dll in plugin dir of VLC" )
set( VLC_INCLUDE_DIR "${VLC_PROGRAM_DIR}/sdk/include/vlc/plugins" CACHE PATH "path to VLC core source files" )
set( VLC_LIB_DIR "${VLC_PROGRAM_DIR}/sdk/lib" CACHE PATH "path to VLC core lib files" )
IF(WIN32)
set( VTMDEC_LIB_NAME "libvtmdec" CACHE PATH "name of the VTM decoder lib" )
set( VLC_LIBs "${VLC_LIB_DIR}/libvlccore.lib" )
ELSE()
set( VTMDEC_LIB_NAME "libvtmdec.so" CACHE PATH "name of the VTM decoder lib" )
set( VLC_LIBs "${VLC_LIB_DIR}/libvlccore.so" )
ENDIF()

# add executable
add_library( ${LIB_NAME} SHARED ${SRC_FILES} ${INC_FILES} ${NATVIS_FILES} )

target_include_directories( ${LIB_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${VLC_INCLUDE_DIR}" "${VTM_DIR}/source/App/LibVTMDec")

file( GLOB SRC_FILES "*.cpp" )

target_link_directories( ${LIB_NAME} PRIVATE "${VTM_DIR}/lib")
target_link_libraries( ${LIB_NAME} PRIVATE ${VLC_LIBs} ${VTMDEC_LIB_NAME} )

# set the folder where to place the projects
IF(WIN32)
set_target_properties( ${LIB_NAME}  PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${VLC_PROGRAM_DIR}/plugins/codec"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${VLC_PROGRAM_DIR}/plugins/codec"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${VLC_PROGRAM_DIR}/plugins/codec"
)
SET_TARGET_PROPERTIES(${LIB_NAME} PROPERTIES LINK_FLAGS "/DELAYLOAD:${VTMDEC_LIB_NAME}.dll")
target_compile_definitions( ${LIB_NAME} PRIVATE VTM_LIB_NAME="${VTMDEC_LIB_NAME}.dll" )
ELSEIF( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  add_custom_command( TARGET ${LIB_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
                                                          ${CMAKE_BINARY_DIR}/lib${LIB_NAME}.so
                                                          ${VLC_PROGRAM_DIR}/modules/.libs/lib${LIB_NAME}.so)
  add_custom_command( TARGET ${LIB_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy 
                                                          ${VTM_DIR}/lib/${VTMDEC_LIB_NAME}
                                                          ${VLC_PROGRAM_DIR}/modules/.libs/${VTMDEC_LIB_NAME})
ENDIF()

target_compile_definitions( ${LIB_NAME} PRIVATE MODULE_STRING="vvcdecoder" __PLUGIN__ )

if( MSVC )
  target_compile_options( ${LIB_NAME} PRIVATE "-WX-" )
else()
  target_compile_options( ${LIB_NAME} PRIVATE "-Wno-error" )
endif()

# Install
#INSTALL(FILES ${LIB_NAME}
#	DESTINATION bin)
#INSTALL(TARGETS ${LIB_NAME} RUNTIME DESTINATION "/")
#	RUNTIME DESTINATION bin
#	LIBRARY DESTINATION lib
#	ARCHIVE DESTINATION lib)