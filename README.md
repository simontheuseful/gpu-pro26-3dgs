# gpu-pro26-3dgs

## Set-Up
1. Download vcpkg and VulkanSDK
2. Set VCPKG_ROOT in your system environment variables
3. cmake --preset vcpkg

if the *build/* directory has not yet been created
- *cmake -B build*
- *cmake --build build*
- if something fails, try removing *build/* and running *>Devel8oper: Reload Window* in VSCode
- cmake generator uses "Visual Studio 17 2022" -> the *.sln* file is inside build/ 


## Output
- the *.exe* file will be located in *build/bin/*

### Adding new files
if intelliSense does not check the code, run:
- `>Cmake: Configure (in VSCode)`