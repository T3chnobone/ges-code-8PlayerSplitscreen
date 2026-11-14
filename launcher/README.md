# GoldenEyeSource.exe launcher

`GoldenEyeSource.exe` lets players start GoldenEye: Source like a standalone game, from a
desktop shortcut, without going through the Steam library.

GES is a Source SDK 2013 mod. Valve's engine can't be shipped with the mod, so it still runs
on **Source SDK Base 2013 Multiplayer**, which is free on Steam. The launcher:

1. finds Steam, and the SDK Base in any Steam library folder;
2. offers to install the SDK Base through Steam if it's missing;
3. finds the GES files: the folder the exe is in (or its `gesource` subfolder) when it holds
   `gameinfo.txt`, otherwise `Steam\steamapps\sourcemods\gesource`;
4. starts `hl2.exe -steam -game "<GES folder>"`, passing on any extra arguments
   (for example `-windowed -noborder -w 3840 -h 1080` for split screen across two monitors).

Steam has to be installed and signed in.

## Build

MinGW (works from Linux too):

```
x86_64-w64-mingw32-g++ -O2 -s -municode -mwindows -static ges_launcher.cpp -o GoldenEyeSource.exe -lshlwapi
```

MSVC:

```
cl /O2 /EHsc ges_launcher.cpp /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib advapi32.lib shlwapi.lib
```
