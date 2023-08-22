#include "LightProbe.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/BufferUtil.hpp"
#include "Vulkan/Image.hpp"
#include "Vulkan/ImageMemoryBarrier.hpp"
#include "Vulkan/ImageView.hpp"
#include "Vulkan/PipelineLayout.hpp"
#include "Vulkan/SingleTimeCommands.hpp"
#include "Vulkan/SwapChain.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Sampler.hpp"

namespace Vulkan::RayTracing {

	LightProbe::LightProbe(glm::vec3 position, const Device& device)
		: position(position)
	{
		radianceDistribution = new ProbeTexture;
		squaredDistances = new ProbeTexture;
		sphericalDistances = new ProbeTexture;



		VkExtent2D radianceExtent;

		radianceExtent.height = 1024;
		radianceExtent.width = 1024;

		VkFormat radianceFormat;
		radianceFormat = VK_FORMAT_B8G8R8A8_UNORM;


		VkExtent2D sphericalExtent;

		sphericalExtent.height = 16;
		sphericalExtent.width = 16;

		VkFormat sphericalFormat;
		sphericalFormat = VK_FORMAT_R16G16_SFLOAT;


		VkExtent2D squaredExtent;

		squaredExtent.height = 16;
		squaredExtent.width = 16;

		VkFormat squaredFormat;
		squaredFormat = VK_FORMAT_R16G16_SFLOAT;


		const auto tiling = VK_IMAGE_TILING_OPTIMAL;

		radianceDistribution->probeImage.reset(new Image(device, radianceExtent, radianceFormat, tiling, VK_IMAGE_USAGE_STORAGE_BIT| VK_IMAGE_USAGE_TRANSFER_SRC_BIT| VK_IMAGE_USAGE_SAMPLED_BIT));
		radianceDistribution->probeImageMemory.reset(new DeviceMemory(radianceDistribution->probeImage->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
		radianceDistribution->probeImageView.reset(new ImageView(device, radianceDistribution->probeImage->Handle(), radianceFormat,VK_IMAGE_ASPECT_COLOR_BIT));
		radianceDistribution->probeSampler.reset(new Sampler(device, Vulkan::RadianceSampler()));

		sphericalDistances->probeImage.reset(new Image(device, sphericalExtent, sphericalFormat, tiling, VK_IMAGE_USAGE_STORAGE_BIT| VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		sphericalDistances->probeImageMemory.reset(new DeviceMemory(sphericalDistances->probeImage->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
		sphericalDistances->probeImageView.reset(new ImageView(device, sphericalDistances->probeImage->Handle(), sphericalFormat, VK_IMAGE_ASPECT_COLOR_BIT));
		sphericalDistances->probeSampler.reset(new Sampler(device, Vulkan::SphericalSampler()));

		squaredDistances->probeImage.reset(new Image(device, squaredExtent, squaredFormat, tiling, VK_IMAGE_USAGE_STORAGE_BIT| VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
		squaredDistances->probeImageMemory.reset(new DeviceMemory(squaredDistances->probeImage->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
		squaredDistances->probeImageView.reset(new ImageView(device, squaredDistances->probeImage->Handle(), squaredFormat, VK_IMAGE_ASPECT_COLOR_BIT));
		sphericalDistances->probeSampler.reset(new Sampler(device, Vulkan::SquaredSampler()));
	}
	LightProbe::~LightProbe()
	{
		radianceDistribution->probeSampler.reset();
		radianceDistribution->probeImage.reset();
		radianceDistribution->probeImageMemory.reset();
		radianceDistribution->probeImageView.reset();
	
		sphericalDistances->probeSampler.reset();
		sphericalDistances->probeImage.reset();
		sphericalDistances->probeImageMemory.reset();
		sphericalDistances->probeImageView.reset();

		squaredDistances->probeSampler.reset();
		squaredDistances->probeImage.reset();
		squaredDistances->probeImageMemory.reset();
		squaredDistances->probeImageView.reset();


		delete radianceDistribution;
		delete sphericalDistances;
		delete squaredDistances;
	}
}