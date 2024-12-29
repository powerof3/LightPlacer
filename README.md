# Light Placer

SKSE plugin and framework for adding lights to objects, using configurable JSON files.
*	[SSE/AE Version](https://www.nexusmods.com/skyrimspecialedition/mods/127557)
*	[VR Version](https://www.nexusmods.com/skyrimspecialedition/mods/135822)

## Requirements
* [CMake](https://cmake.org/)
	* Add this to your `PATH`
* [PowerShell](https://github.com/PowerShell/PowerShell/releases/latest)
* [Vcpkg](https://github.com/microsoft/vcpkg)
	* Add the environment variable `VCPKG_ROOT` with the value as the path to the folder containing vcpkg
* [Visual Studio Community 2019](https://visualstudio.microsoft.com/)
	* Desktop development with C++
* [CommonLibSSE](https://github.com/powerof3/CommonLibSSE/tree/dev)
	* You need to build from the powerof3/dev branch
	* Add this as as an environment variable `CommonLibSSEPath`
* [CommonLibVR](https://github.com/alandtse/CommonLibVR/tree/vr)
	* Add this as as an environment variable `CommonLibVRPath` instead of /extern

## User Requirements
* [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	* Needed for SSE/AE
* [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
	* Needed for VR

## Register Visual Studio as a Generator
* Open `x64 Native Tools Command Prompt`
* Run `cmake`
* Close the cmd window

## Building
```
git clone https://github.com/powerof3/LightPlacer.git
cd LightPlacer
```

### SSE
```
cmake --preset vs2022-windows-vcpkg-se
cmake --build build --config Release
```
### AE
```
cmake --preset vs2022-windows-vcpkg-ae
cmake --build buildae --config Release
```
### VR
```
cmake --preset vs2022-windows-vcpkg-vr
cmake --build buildvr --config Release
```
## License
[MIT](LICENSE)
