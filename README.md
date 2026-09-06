# Light Placer

SKSE plugin and framework for adding lights to objects, using configurable JSON files.
*	[SSE/AE Version](https://www.nexusmods.com/skyrimspecialedition/mods/127557)
*	[VR Version](https://www.nexusmods.com/skyrimspecialedition/mods/135822)

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2022](https://visualstudio.microsoft.com/) or newer
	* Desktop development with C++

## User Requirements
* [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	* Needed for SSE/AE
* [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
	* Needed for VR

## Building
```
git clone https://github.com/powerof3/LightPlacer.git
cd LightPlacer
git submodule update --init --recursive
```

### SSE (1.5.97)
```
cmake --preset vs2022-se
cmake --build --preset vs2022-se
```
### AE (1.6.1170+)
```
cmake --preset vs2022-ae
cmake --build --preset vs2022-ae
```
### VR
```
cmake --preset vs2022-vr
cmake --build --preset vs2022-vr
```

Replace `vs2022` with `vs2026` to build with Visual Studio 2026.

## License
[GPL-3.0](LICENSE)
