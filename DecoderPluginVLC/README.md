/************* HOW TO BUILD THE VVC VLC PLUGIN ************/

Prerequisite:

VVC decoder library libvtmdec
- currently VTM-22.2 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-22.2-MT
- currently VTM-21.2 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-21.2-MT
- currently VTM-20.2 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-20.2-MT
- VTM-18.2 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-18.2-MT
- VTM-17.2 version can be found here:
  https://vcgit.hhi.fraunhofer.de/delagrangep/VVCSoftware_VTM/-/tree/VTM-17.2-MT
- for Windows, a precompiled .dll is available
- otherwise, get the sources and build libvtmdec decoder library (enable option "BUILD_LIBVTMDEC" in CMake).


VLC Pugin:
-------------------
On WINDOWS
-------------------

Prerequisite:
  - VLC SDK
  You can download the binary release with sdk on videolan site: https://get.videolan.org/vlc/
  For example 3.0.20 version: https://get.videolan.org/vlc/3.0.20/, choose the 7z package
  - download and decompress for example on c:/.
  - modify \sdk\include\vlc\plugins\vlc_threads.h file
  Unless you need it (and you'll have to figure out how to compile with it) 
  you should comment the definition of vlc_poll function 

Build the VTM plugin using CMake:
- set libvtmdec related variables:
  VTM_DIR               path to VTM root directory with libvtmdec built
  VTMDEC_LIB_NAME       dll libray file name (default name should be OK)

- set directories for vlc sdk:
  VLC_PROGRAM_DIR       VLC binary directory - used to copy output library in the appropriate plugin directory
  VLC_INCLUDE_DIR       =c:/vlc-3.0.20/sdk/include/vlc/plugins
  VLC_LIB_DIR           =c:/vlc-3.0.20/sdk/lib

- generate the solution and compile
  The dll will be placed in the correct vlc plugin directory (VLC_OUTPUT_PLUGIN_DIR = c:/vlc-3.0.20/plugins)

-------------------
On Linux
-------------------

Prerequisite:
  - VLC SDK
  You can download the binaries release with sdk on videolan site: https://get.videolan.org/vlc/
  For example 3.0.20 version: https://get.videolan.org/vlc/3.0.20/
  - download and decompress the sources
  - build using configure and make
  get missing dependencies, or disable them

Build the VTM plugin using CMake:
- set libvtmdec related variables:
  VTM_DIR               path to VTM root directory with libvtmdec built
  VTMDEC_LIB_NAME       dll libray file name (default name should be OK)

- set directories for vlc sdk:
  VLC_PROGRAM_DIR       VLC binary directory - used to copy output library in the appropriate plugin directory
  VLC_INCLUDE_DIR       =c:/vlc-3.0.20/sdk/include/vlc/plugins
  VLC_LIB_DIR           =c:/vlc-3.0.20/sdk/lib

- generate the solution and compile
  The dll will be placed in the source/bin directory and need to be placed in vlc plugin directory /vlc/modules/.libs/
