# Light Placer

SKSE plugin that adds lights using configurable JSON files or through extradata in nif nodes.

## Format

### JSON
<details>

JSON files must be placed in `Data/LightPlacer` folder
eg. `Data/LightPlacer/YourMod/File1.json`

1. Model Path
```json
[
  {
    "models": [ "architecture\\solitude\\sbluepalaceentrance.nif" ],
    "lightData": []
  }
]
```
2. References
```json
[
  {
    "references": [ "0x12345~Skyrim.esm", "MyRefEDID" ],
    "lightData": []
  }
]
```
3. Addon Nodes
```json
[
  {
    "addonNodes": [ 49 ],
    "lightData": [
      {
        "whiteList": [ "furniture\\enchantingworkbench.nif" ],
        "blackList": [ "furniture\\enchantingworkstation.nif" ],
        "data": {}
      }
    ]
  }
]
```  
#### Light Data

Choose either node names or points to attach lights to.

`"light"` is required, all other fields are optional. 

`"radius"`,`"fade"` values will override values found in the light record.

```json
    "lightData": [
      {
        "points": [ [ 490.335, -152.014, -208.954 ] ],
	"nodeNames": [ "Node1Name", "Node2Name"],
        "data": {
          "light": "TestGrantCandleLight01NS",
          "radius": 192,
          "fade": 2.5
	  "offset": [0,0,100],
	  "externalEmittance": "EDID"
	  "chance": 100          
        }
      }
    ]
```
</details>

### NIF
<details>

Add `NiStringsExtraData` extraData to your node, named `LIGHT_PLACER`

[0] = LightEDID

[1] = Radius

[2] = Fade

![image](https://i.imgur.com/E7nUYvg.png)
	
</details>

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

## User Requirements
* [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
	* Needed for SSE/AE

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
## License
[MIT](LICENSE)
