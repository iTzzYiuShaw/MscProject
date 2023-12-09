# Msc Proejct- Simple Light probes

## Building

First you will need to install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). For Windows, LunarG provides installers. For Ubuntu LTS, they have native packages available. For other Linux distributions, they only provide tarballs. The rest of the third party dependencies can be built using [Microsoft's vcpkg](https://github.com/Microsoft/vcpkg) as provided by the scripts below.

If in doubt, please check the GitHub Actions [continuous integration configurations](.github/workflows) for more details.

**Windows (Visual Studio 2022 x64 solution)** [![Windows CI Status](https://github.com/GPSnoopy/RayTracingInVulkan/workflows/Windows%20CI/badge.svg)](https://github.com/GPSnoopy/RayTracingInVulkan/actions?query=workflow%3A%22Windows+CI%22)
```
vcpkg_windows.bat
build_windows.bat
```
**Linux (GCC 9+ Makefile)** [![Linux CI Status](https://github.com/GPSnoopy/RayTracingInVulkan/workflows/Linux%20CI/badge.svg)](https://github.com/GPSnoopy/RayTracingInVulkan/actions?query=workflow%3A%22Linux+CI%22)

For example, on Ubuntu 20.04 :
```
sudo apt-get install curl unzip tar libxi-dev libxinerama-dev libxcursor-dev xorg-dev
./vcpkg_linux.sh
./build_linux.sh
```


## Light Probes

“See” the actual scene from the perspective of the probe

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/deb3fad2-050b-4152-bcba-c64cbac6a413)

Radiance distribution texture

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/a5cca0f8-516c-4d31-ab06-bf30a694401c)


### Sampling result

Original Raytracing (Front)

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/40d1108d-a2a5-47e0-91aa-a51e2020ac5a)

Light probes Sampling (Front)

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/abfd87dd-83f1-4d4a-9846-981575daf9db)

