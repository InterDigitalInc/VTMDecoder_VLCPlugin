project(TsDemuxVVC_plugin)
# minimum required cmake version
cmake_minimum_required( VERSION 3.5 FATAL_ERROR )
# executable
IF(WIN32)
set( LIB_NAME libvvctsdemux_plugin )
else()
set( LIB_NAME vctsdemux_plugin )
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
set( CMAKE_C_STANDARD 11 )
set( C_STANDARD_REQUIRED 11 )

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
  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  # ----------------------------------------------------------------------------
# Use statically or dynamically linked CRT?
# Default: static
# ----------------------------------------------------------------------------
if(WIN32)
    OPTION (BUILD_WITH_STATIC_CRT "Enables use of staticaly linked CRT" ON)
    if(BUILD_WITH_STATIC_CRT)
        foreach(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
           if(${flag_var} MATCHES "/MD")
              string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
           endif()
           if(${flag_var} MATCHES "/MDd")
              string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
           endif()
        endforeach(flag_var)
        
    else(BUILD_WITH_STATIC_CRT)
        foreach(flag_var
                CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
           if(${flag_var} MATCHES "/MT")
              string(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
           endif()
           if(${flag_var} MATCHES "/MTd")
              string(REGEX REPLACE "/MTd" "/MDd" ${flag_var} "${${flag_var}}")
           endif()
        endforeach(flag_var)
    endif(BUILD_WITH_STATIC_CRT)
endif()
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
file( GLOB SRC_FILES "*.cpp" "*.c" )
# get all source files

set( VLC_PROGRAM_DIR "C:/VLC" CACHE PATH "will output dll in plugin dir of VLC" )
set( VLC_INCLUDE_DIR "${VLC_PROGRAM_DIR}/sdk/include/vlc/plugins" CACHE PATH "path to VLC core source files" )
set( VLC_LIB_DIR "${VLC_PROGRAM_DIR}/sdk/lib" CACHE PATH "path to VLC core lib files" )
set( VLC_SRC_DIR "vlc/src" CACHE PATH "path to VLC src dir" )
set( DVBPSI_DIR "dvbpsi" CACHE PATH "path to dvbpsi dir (ex: version 1.3.2)" )

set( SRC_FILES "${SRC_FILES}"
 "${VLC_SRC_DIR}/modules/demux/mpeg/ts.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_pid.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_pid_fwd.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_pid.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_psi.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_si.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_si.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_psip.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_psip.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_psip_dvbpsi_fixes.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_psip_dvbpsi_fixes.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_decoders.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_decoders.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_streams.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_streams.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_scte.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_scte.c;\
${VLC_SRC_DIR}/modules/demux/mpeg/sections.c;${VLC_SRC_DIR}/modules/demux/mpeg/sections.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/mpeg4_iod.c;${VLC_SRC_DIR}/modules/demux/mpeg/mpeg4_iod.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_arib.c;${VLC_SRC_DIR}/modules/demux/mpeg/ts_arib.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_sl.c;${VLC_SRC_DIR}/modules/demux/mpeg/ts_sl.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_metadata.c;${VLC_SRC_DIR}/modules/demux/mpeg/ts_metadata.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_hotfixes.c;${VLC_SRC_DIR}/modules/demux/mpeg/ts_hotfixes.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/ts_strings.h;${VLC_SRC_DIR}/modules/demux/mpeg/ts_streams_private.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/pes.h;\
${VLC_SRC_DIR}/modules/demux/mpeg/timestamps.h;\
${VLC_SRC_DIR}/modules/demux/dvb-text.h;\
${VLC_SRC_DIR}/modules/demux/opus.h;\
${VLC_SRC_DIR}/modules/mux/mpeg/csa.c;\
${VLC_SRC_DIR}/modules/mux/mpeg/dvbpsi_compat.h;\
${VLC_SRC_DIR}/modules/mux/mpeg/streams.h;\
${VLC_SRC_DIR}/modules/mux/mpeg/tables.c;${VLC_SRC_DIR}/modules/mux/mpeg/tables.h;\
${VLC_SRC_DIR}/modules/mux/mpeg/tsutil.c;${VLC_SRC_DIR}/modules/mux/mpeg/tsutil.h;\
${VLC_SRC_DIR}/modules/access/dtv/en50221_capmt.h;\
${VLC_SRC_DIR}/modules/codec/jpeg2000.h;${VLC_SRC_DIR}/modules/codec/scte18.h;\
${VLC_SRC_DIR}/modules/codec/atsc_a65.c;${VLC_SRC_DIR}/modules/codec/atsc_a65.h;\
${VLC_SRC_DIR}/modules/codec/opus_header.c" )

file( GLOB DVBPSI_SRC_FILES "${DVBPSI_DIR}/src/*.c" "${DVBPSI_DIR}/src/tables/*.c" "${DVBPSI_DIR}/src/descriptors/*.c" "${DVBPSI_DIR}/src/descriptors/types/*.c" )


IF(WIN32)
set( VLC_OUTPUT_PLUGIN_DIR "C:/VLC/plugins/" CACHE PATH "path to VLC plugin dir for output binary" )
ENDIF()
set(VLC_LIBs "${VLC_LIB_DIR}/libvlccore.lib" )

# add executable
add_library( dvbpsi ${DVBPSI_SRC_FILES} )
add_library( ${LIB_NAME} SHARED ${SRC_FILES} )

target_include_directories( ${LIB_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${VLC_SRC_DIR}/modules/demux" "${VLC_INCLUDE_DIR}" "./" "${DVBPSI_DIR}/src/descriptors/")
target_include_directories( ${LIB_NAME} PRIVATE "${VLC_SRC_DIR}/modules/demux/mpeg/")

target_include_directories( dvbpsi PRIVATE "./")


target_link_libraries( ${LIB_NAME} ${VLC_LIBs} dvbpsi )


if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  add_custom_command( TARGET ${LIB_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
                                                          $<$<CONFIG:Debug>:${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG}/lib${LIB_NAME}.so>
                                                          $<$<CONFIG:Release>:${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE}/lib${LIB_NAME}.so>
                                                          $<$<CONFIG:Debug>:${VLC_PROGRAM_DIR}/modules/.libs/lib${LIB_NAME}.so>
                                                          $<$<CONFIG:Release>:${VLC_PROGRAM_DIR}/modules/.libs/lib${LIB_NAME}.so>)
endif()


# set the folder where to place the projects
IF(WIN32)
set_target_properties( ${LIB_NAME}  PROPERTIES 
    FOLDER app
    RUNTIME_OUTPUT_DIRECTORY "${VLC_PROGRAM_DIR}/plugins/demux"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${VLC_PROGRAM_DIR}/plugins/demux"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${VLC_PROGRAM_DIR}/plugins/demux"
)
ENDIF()

target_compile_definitions( ${LIB_NAME} PRIVATE MODULE_STRING="vvctsdemux" __PLUGIN__ )

if( MSVC )
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/dvbpsi.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/demux.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/descriptor.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/psi.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/pat.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/sdt.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/pmt.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/eit.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/tot.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/atsc_stt.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/atsc_mgt.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/atsc_vct.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/atsc_eit.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/tables/atsc_ett.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/descriptors/dr.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
  add_custom_command( TARGET dvbpsi POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                                          ${DVBPSI_DIR}/src/descriptors/dr_a0.h ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}dvbpsi/)
														  
  SET_SOURCE_FILES_PROPERTIES("${DVBPSI_DIR}/src/tables/sis.c" PROPERTIES LANGUAGE CXX )
  target_compile_definitions( ${LIB_NAME} PRIVATE HAVE_CONFIG_H )
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

