#pragma once
#include "glm/glm.hpp"
#include "Assets/TextureImage.hpp"
#include <memory>
#include "Vulkan/Vulkan.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/Sampler.hpp"

namespace Vulkan::RayTracing {



	struct ProbeTexture
	{

		std::unique_ptr<Image> probeImage;
		std::unique_ptr<DeviceMemory> probeImageMemory;
		std::unique_ptr<ImageView> probeImageView;
		std::unique_ptr<Sampler> probeSampler;

		const Vulkan::Sampler& Sampler() const { return *probeSampler; }
	};

	class LightProbe
	{

	public:
		using Ptr = std::shared_ptr<LightProbe>;

		static Ptr create(glm::vec3 position, const Device& device)
		{
			return std::make_shared<LightProbe>(position, device);
		}


		LightProbe(glm::vec3 position,const Device& device);
		~LightProbe();

	public:

		// 3D position of the light probe in the world
		glm::vec3 position;

		// Local incident radiance distribution for each direction
		// This could be a pointer to a texture object or some other representation
		ProbeTexture* radianceDistribution;

		// Spherical distance to the nearest geometry for each direction
		// This could be a pointer to a texture object or some other representation
		ProbeTexture* sphericalDistances;

		// Squared distance to the nearest geometry for each direction
		// This could be a pointer to a texture object or some other representation
		ProbeTexture* squaredDistances;
	};
}




