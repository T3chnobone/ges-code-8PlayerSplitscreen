# Shader Editor for the current engine

GoldenEye: Source draws some materials with Biohazard90's
[Source Shader Editor](https://github.com/Biohazard90/source-shader-editor): Facility and Stack
glass, Bunker's map and monitor screens, Silo's screens, Aztec's screens and light shafts, and
Complex's walls. Their `.vmt` files use the `editor_shader` shader, and the compiled shaders ship
in `shaders\fxc` and `shadereditorui`.

The editor is two DLLs in `gesource\bin`:

| DLL | What it is |
|---|---|
| `shadereditor.dll` | the editor runtime. `client.dll` loads it (`game/client/ShaderEditor`). |
| `game_shader_generic_procshader.dll` | the material system shader DLL with `EDITOR_SHADER`. The engine loads it. |

The 2016 builds that come with GoldenEye: Source 5.0.6 don't load with the current Source SDK
Base 2013 Multiplayer engine: `shadereditor.dll` imports `CThread::WaitForCreateComplete` as a
virtual function, and the current `tier0.dll` only exports it as a normal one. Without the
editor, drawing an `editor_shader` material crashed the game. This folder rebuilds both DLLs from
source against this branch's headers. The client still falls back to standard shaders if the
editor doesn't load (`GESS_ReplaceEditorMaterials` in `ShaderEditorSystem.cpp`).

## Build

Build the main project first (this uses its `tier1.lib`). Then:

```
git clone https://github.com/Biohazard90/source-shader-editor.git
cd source-shader-editor
git checkout 2c20e1b49a9cc80cabe0145bf51b78b7193d2a49
git apply <ges-code>/shadereditor/ges-sse.patch
cmake -S <ges-code>/shadereditor -B build -G "Visual Studio 17 2022" -A Win32 "-DSSE_ROOT=<source-shader-editor>" "-DGE_TIER1_LIB=<ges-code build>/tier1/Release/tier1.lib"
cmake --build build --config Release
```

Output: `build\Release\shadereditor.dll` and `build\Release\game_shader_generic_procshader.dll`.
Copy both to `sourcemods\gesource\bin`.

`ges-sse.patch` changes three lines:

- `dll_init.cpp`: load the shader DLL by GoldenEye's name, `game_shader_generic_procshader.dll`.
- `procshader.cpp`: `LightState_t::m_bStaticLight` is `m_bStaticLightVertex` in the 2013 SDK.
- `perforcefileexplorer.cpp`: `CUtlString::Get()` is const in the 2013 SDK.

`CMakeLists.txt` replaces the VPC projects (`shadereditor.vpc`, `vgui_controls_editor.vpc`,
`procshader.vpc`); `sources.cmake` is their file lists. It links the 32-bit libraries in
`lib/public` (the current SDK has only 64-bit ones). The interfaces these DLLs use (material
system, shader API, `ITexture`) only had methods added at the end in the 2025 update, so the
older headers and libraries still match the engine.

The Shader Editor is under the Source 1 SDK license (`LICENSE` in its repository).
