project(gesource)

cmake_minimum_required(VERSION 2.8.12)
set(GESOURCE_VERSION "5.0.0")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include("ucm")
include("ExternalProject")

# Default to a release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE
        STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Global definitions
add_definitions(-DVPROF_LEVEL=1 -DSWDS -D_snprintf=use_Q_snprintf_instead -DRAD_TELEMETRY_DISABLED -D_USRDLL -DVECTOR)

# Platform specific definitions
if(CMAKE_HOST_UNIX)
  add_definitions(-D_LINUX -DLINUX -D_POSIX -DPOSIX -DGNUC -DCOMPILER_GCC -DVPROF_LEVEL=1 -DGNUC -DNO_HOOK_MALLOC
    -DNO_MALLOC_OVERRIDE -D_DLL_EXT=.so)

  add_compile_options(-w -std=gnu++0x -fpermissive -fno-strict-aliasing -ffast-math -fno-omit-frame-pointer
    -ftree-vectorize -fpredictive-commoning -funswitch-loops -m32 -march=core2 -mtune=core2 -msse2 -mfpmath=sse)

#  add_compile_options(-Wall -Wno-invalid-offsetof -Wno-multichar -Wno-overloaded-virtual -Wno-narrowing -Wno-write-strings
#  	-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-local-typedefs -Wno-unused-result
#    -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers
#    -Wno-sign-compare -Wno-reorder -Wno-invalid-offsetof -Wno-float-equal -Werror=return-type -Wno-attributes
#    -fdiagnostics-show-option -Wformat -Wformat-security)
else()
  # Windows (MSVC, 32-bit). Settings follow vpc_scripts/source_dll_win32_release.vpc.
  add_definitions(-DWIN32 -D_WIN32 -D_WINDOWS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_DLL_EXT=.dll
    -DCOMPILER_MSVC -DCOMPILER_MSVC32 -DNDEBUG -D_ALLOW_RUNTIME_LIBRARY_MISMATCH -D_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
    -D_ALLOW_MSC_VER_MISMATCH -DNO_WARN_MBCS_MFC_DEPRECATION)
  # Static CRT (/MT) like the VPC build
  foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
  endforeach()
  add_compile_options(/MP /W0 /fp:fast /GR /Zc:forScope /Zm200 /Zc:threadSafeInit- /EHsc /Oy- /Z7 /permissive)
  add_link_options(/SAFESEH:NO /DEBUG /OPT:REF /OPT:ICF
    /NODEFAULTLIB:libc /NODEFAULTLIB:libcd /NODEFAULTLIB:libcmtd /NODEFAULTLIB:libcpmtd)
endif()

if(WIN32)
  # Prebuilt dependencies: Boost headers + Boost.Python sources, and Python matching the game's python34.dll
  set(GES_BOOST_ROOT "" CACHE PATH "Boost source tree (headers and libs/python/src)")
  set(GES_PYTHON_ROOT "" CACHE PATH "Python 3.4 (32-bit) install with include/ and libs/")
  include_directories("${GES_BOOST_ROOT}" "${GES_PYTHON_ROOT}/include" "${CMAKE_SOURCE_DIR}/cmake/msvc_compat")
  link_directories("${GES_PYTHON_ROOT}/libs")
  add_custom_target(python)
else()

# Boost package defines
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.54.0)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "Could not find Boost libraries. Download and install at least version 1.54.0 to continue.")
endif()

# Configure, build, and "install" Python
ExternalProject_Add(python
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/python"
  CMAKE_ARGS "-C${CMAKE_SOURCE_DIR}/cmake/ges_python.cmake" 
	"-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/python/"
    "-DCMAKE_C_FLAGS=-m32"
  BUILD_COMMAND ""
  INSTALL_COMMAND "${CMAKE_COMMAND}"
      --build .
      --target install
      --config Release
  PREFIX "python"
)

include_directories("${CMAKE_BINARY_DIR}/python/include/python3.5/")
link_directories("${CMAKE_BINARY_DIR}/python/lib/")
endif()

# Add in the projects
add_subdirectory("thirdparty/boost-python")
#add_subdirectory("mathlib")
add_subdirectory("tier1")
add_subdirectory("vgui2")
add_subdirectory("game")
