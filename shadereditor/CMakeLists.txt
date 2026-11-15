cmake_minimum_required(VERSION 3.21)
project(ges_shadereditor CXX C)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Builds Biohazard90's Source Shader Editor for GoldenEye: Source on the current
# Source SDK Base 2013 Multiplayer engine (32-bit):
#   shadereditor.dll                    - the editor runtime the client loads
#   game_shader_generic_procshader.dll  - the material system shader DLL (EDITOR_SHADER)
get_filename_component(_ge_default "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GE "${_ge_default}" CACHE PATH "ges-code checkout (public/, common/, lib/public)")
set(SSE_ROOT "" CACHE PATH "source-shader-editor checkout (commit 2c20e1b) with ges-sse.patch applied")
if(NOT EXISTS "${SSE_ROOT}/src/shadereditor/shadereditor.vpc")
  message(FATAL_ERROR "Set SSE_ROOT to a source-shader-editor checkout (see README.md)")
endif()
set(SSE "${SSE_ROOT}/src")
set(GE_TIER1_LIB "" CACHE FILEPATH "tier1.lib built by the ges-code build")

include("${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake")

foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE)
  string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
endforeach()
add_compile_options(/MP /W0 /fp:fast /GR /Zc:forScope /Zm200 /Zc:threadSafeInit- /EHsc /Oy- /Z7 /permissive)
add_compile_definitions(WIN32 _WIN32 _WINDOWS NDEBUG _USRDLL _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE
  _ALLOW_RUNTIME_LIBRARY_MISMATCH _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH _ALLOW_MSC_VER_MISMATCH
  COMPILER_MSVC COMPILER_MSVC32 _DLL_EXT=.dll VECTOR RAD_TELEMETRY_DISABLED NO_WARN_MBCS_MFC_DEPRECATION
  SHADER_EDITOR_DLL SHADER_EDITOR_DLL_2013)
add_link_options(/SAFESEH:NO /DEBUG /OPT:REF /OPT:ICF
  /NODEFAULTLIB:libc /NODEFAULTLIB:libcd /NODEFAULTLIB:libcmtd /NODEFAULTLIB:libcpmtd)

set(GE_LIBS "${GE}/lib/public")
set(COMMON_INC "${GE}/public" "${GE}/public/tier0" "${GE}/public/tier1" "${GE}/common"
  "${GE}/cmake/msvc_compat")

# --- vgui_controls_editor (static) ---------------------------------------------
add_library(vgui_controls_editor STATIC ${VG_SOURCES})
target_include_directories(vgui_controls_editor PRIVATE "${SSE}/vgui_editor" "${SSE}/shadereditor" ${COMMON_INC})
target_compile_definitions(vgui_controls_editor PRIVATE _LIB)

# --- shadereditor.dll ----------------------------------------------------------
add_library(shadereditor SHARED ${SE_SOURCES} "${GE}/public/tier0/memoverride.cpp"
  "${SSE}/vgui_editor/vgui_controls_editor.cpp" "${GE}/cmake/msvc_compat/iob_func_shim.cpp")
set(SED "${SSE}/shadereditor")
target_include_directories(shadereditor PRIVATE "${SSE}/vgui_editor" "${SED}" "${SED}/util" "${SED}/interface"
  "${SSE_ROOT}/client/shadereditor" "${SED}/nodes" "${SED}/properties" "${SED}/solver" "${SED}/threads"
  "${SED}/solver/postprocessing" "${SED}/properties/postprocessing" "${SED}/properties/smarttext"
  "${SED}/nodes/postprocessing" "${SSE}/materialsystem" ${COMMON_INC})
target_compile_definitions(shadereditor PRIVATE NO_STRING_T CLIENT_DLL VERSION_SAFE_STEAM_API_INTERFACES
  PROTECTED_THINGS_ENABLE strncpy=use_Q_strncpy_instead _snprintf=use_Q_snprintf_instead ENABLE_CHROMEHTMLWINDOW
  fopen=dont_use_fopen DLLNAME=shadereditor)
target_link_libraries(shadereditor PRIVATE vgui_controls_editor
  "${GE_LIBS}/bitmap.lib" "${GE_LIBS}/choreoobjects.lib" "${GE_LIBS}/dmxloader.lib" "${GE_LIBS}/mathlib.lib"
  "${GE_LIBS}/matsys_controls.lib" "${GE_LIBS}/particles.lib" "${GE_TIER1_LIB}" "${GE_LIBS}/tier2.lib"
  "${GE_LIBS}/tier3.lib" "${GE_LIBS}/steam_api.lib" "${GE_LIBS}/vtf.lib" "${SSE}/lib/public/jpeglib.lib"
  "${GE_LIBS}/tier0.lib" "${GE_LIBS}/vstdlib.lib"
  legacy_stdio_definitions winmm shell32 user32 advapi32 gdi32 comdlg32 ole32)
target_link_options(shadereditor PRIVATE /FORCE:MULTIPLE)

# --- game_shader_generic_procshader.dll ----------------------------------------
set(PSD "${SSE}/materialsystem/procshader")
add_library(game_shader_generic_procshader SHARED
  "${PSD}/basevsshader.cpp" "${PSD}/nodeshader.cpp" "${PSD}/procshader.cpp" "${PSD}/prev.cpp"
  "${SSE}/materialsystem/ivprocshader.cpp" "${SSE}/materialsystem/procshaderinterface.cpp"
  "${GE}/public/tier0/memoverride.cpp")
target_include_directories(game_shader_generic_procshader PRIVATE "${PSD}" "${PSD}/fxctmp9" "${PSD}/vshtmp9"
  "${SSE}/materialsystem" "${SSE_ROOT}/client/shadereditor" ${COMMON_INC})
target_compile_definitions(game_shader_generic_procshader PRIVATE STDSHADER_DX9_DLL_EXPORT FAST_MATERIALVAR_ACCESS
  GAME_SHADER_DLL PROCSHADER_DLL USE_ACTUAL_DX DLLNAME=game_shader_generic_procshader)
target_link_libraries(game_shader_generic_procshader PRIVATE "${GE_LIBS}/mathlib.lib" "${GE_LIBS}/shaderlib.lib"
  "${GE_TIER1_LIB}" "${GE_LIBS}/tier0.lib" "${GE_LIBS}/vstdlib.lib"
  legacy_stdio_definitions winmm shell32 user32 advapi32 gdi32 comdlg32 ole32)
target_link_options(game_shader_generic_procshader PRIVATE /FORCE:MULTIPLE)
