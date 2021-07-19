/************* HOW TO BUILD THE VVC VLC PLUGIN ************/

Prerequisite: 

VVC library DecVTMLib
  currently VTM-13 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-13.0-MT

  get the sources and build DecVTMLib decoder library (enable option "BUILD_VTM_LIB" in CMake)

VLC Pugin:
-------------------
On WINDOWS
-------------------

Prerequisite: 
  - VLC SDK
  You can download the binary release with sdk on videolan site: https://get.videolan.org/vlc/
  For example 3.0.9.2 version: https://get.videolan.org/vlc/3.0.9.2/
  - download and decompress for example on c:/.
  - modify \sdk\include\vlc\plugins\vlc_threads.h file
  Unless you need it (and you'll have to figure out how to compile with it) 
  you should comment the definition of vlc_poll function 


Build the VTM plugin using CMake.
- set DecVTMLib:
  VTM_DIR               path to VTM root directory with DecVTMLib built
  DecVTMLib_Name        dll libray file name (default name should be OK)
  
- set directories for vlc sdk:
  VLC_PROGRAM_DIR       VLC binary directory - used to copy output library in the appropriate plugin directory
  VLC_INCLUDE_DIR       =c:/vlc-3.0.9.2/sdk/include/vlc/plugins
  VLC_LIB_DIR           =c:/vlc-3.0.9.2/sdk/lib
- generate the solution and compile
  The dll will be placed in the correct vlc plugin directory (VLC_OUTPUT_PLUGIN_DIR =c:/vlc-3.0.9.2/plugins)
  
-------------------
On Linux
-------------------

Prerequisite: you need VLC SDK
You can download the binaries release with sdk on videolan site: https://get.videolan.org/vlc/
For example 3.0.9.2 version: https://get.videolan.org/vlc/3.0.9.2/
- download and decompress the sources
- build using configure and make
  get missing dependencies, or disable them

Build the VTM using CMake.
- set DecVTMLib dependency:
  VTM_DIR               path to VTM root directory with DecVTMLib built
  DecVTMLib_Name        dll libray file name (default name should be OK)
  
- set directories for vlc sdk:
  VLC_PROGRAM_DIR       VLC binary directory - used to copy output library in the appropriate plugin directory
  VLC_INCLUDE_DIR       =c:/vlc-3.0.9.2/sdk/include/vlc/plugins
  VLC_LIB_DIR           =c:/vlc-3.0.9.2/sdk/lib
- generate the solution and compile
  The dll will be placed in the source/bin directory and need to be placed in vlc plugin directory /vlc/modules/.libs/
  
  