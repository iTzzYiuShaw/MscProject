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


## 1: Capturing Realistic Lighting Information
Real-time rendering systems need to quickly and accurately capture the information of in-direct lighting, which can be influenced by multiple sources and can interact with various surfaces. After that, the lighting information will be stored into textures of light probes, which will be used in the actual rendering process. 

For the task of capturing realistic lighting information in this project, ray tracing has been chosen as our primary method. The chosen raytracing API is **VK_KHR_ray_tracing**. 

Ray tracing complements the concept of light probes. By shooting rays from the probe positions, we can capture a complete lighting environment, ensuring that the probes store accurate and high-quality lighting data.

When the primary ray intersects with an object, it performs different forms of scatter based on the material properties of the object. 
This returns specific lighting information and a new reflection, prompting the generation of secondary rays. In the hitting stage of ray tracing, the vertices of the current triangle are unpacked and used to calculate the texture coordinates of the hit point.

Due to the variability in the directions of different secondary rays produced by each bounce, multiple samples of the same primary ray are necessary. This ensures the quality and stability of the lighting information and is known as **Monte Carlo integration** in ray tracing. 

## 2: Octahedral Mapping
Octahedral mapping is a technique that allows us to encode and decode 3D unit vectors into 2D coordinates. The main idea is to project the 3D unit vectors onto an octahedron, and then unfold the octahedron onto a 2D plane.

In the pre-computation phase, each 2D UV coordinate of a light probe texture uniquely maps to a direction vector on a 3D sphere. This direction vector serves as the initial direction for the primary ray in the ray tracing process. The ray tracing simulation then proceeds along this direction, interacting with the scene's geometry and materials. The resulting color, which encapsulates the lighting information from that direction, then it is stored back into the corresponding pixel of the light probe texture. 

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/1652095e-e8e4-4f5a-8d48-bf46baa5376a)

## 3: Sampling phase
Rays are cast into the scene from the camera's perspective. For each ray that intersects a surface, the intersection point’s normal, material color, and position are recorded. 

For each intersection point, rays are cast towards all light probes to perform occlusion tests. Probes with a clear line of sight to the intersection point are considered valid ones. For each of these probes, the system calculates the distance and direction to the intersection point, subsequently computing and storing the corresponding distance and angle weights. The distance and angle weights are calculated as follows:

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/c6f22fe7-8802-4c4f-b1f6-336095fb5e1c)

What needs to be noticed in the occlusion test is that in the context of ray tracing, t is an extremely important parameter. This value of t tells us two important pieces of information:
By substituting the value of t into the ray parameter equation, we can precisely calculate the 3D coordinates of the hitting point where the ray hits the object by the following equation:

![image](https://github.com/iTzzYiuShaw/MscProject_LightProbes/assets/110170509/969933e0-e7ef-4e0d-ac87-9ac75cf6f339)

Where P(t) is the intersection point or hit point on the ray at parameter t, O is the starting point of the ray, D is the normalized direction of the ray, and t is a scalar parameter that stands for the distance from the ray's origin. Therefore, based on the value of t, we can determine whether there is an obstacle between hit point and the light probe. For example, if t is a negative number, it means the ray does not hit anything, which means that there is no blocking, and this light probe can “see” this hit point and record its lighting information.
Next, the system selects the two probes with the highest combined weights. For each chosen probe, the angle between the probe and the intersection point is used to derive a 2D UV coordinate, mirroring the inverse of the octahedral mapping used in the first phase. 


