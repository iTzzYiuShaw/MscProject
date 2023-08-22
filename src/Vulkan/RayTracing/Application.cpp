#include "Application.hpp"
#include "BottomLevelAccelerationStructure.hpp"
#include "DeviceProcedures.hpp"
#include "RayTracingPipeline.hpp"
#include "ShaderBindingTable.hpp"
#include "TopLevelAccelerationStructure.hpp"
#include "LightProbeRTPipeline.hpp"
#include "LightProbe.hpp"
#include "Assets/Model.hpp"
#include "Assets/Scene.hpp"
#include "Utilities/Glm.hpp"
#include "Vulkan/Buffer.hpp"
#include "Vulkan/BufferUtil.hpp"
#include "Vulkan/Image.hpp"
#include "Vulkan/ImageMemoryBarrier.hpp"
#include "Vulkan/ImageView.hpp"
#include "Vulkan/PipelineLayout.hpp"
#include "Vulkan/SingleTimeCommands.hpp"
#include "Vulkan/SwapChain.hpp"
#include <chrono>
#include <iostream>
#include <numeric>


namespace Vulkan::RayTracing {

namespace
{
	template <class TAccelerationStructure>
	VkAccelerationStructureBuildSizesInfoKHR GetTotalRequirements(const std::vector<TAccelerationStructure>& accelerationStructures)
	{
		VkAccelerationStructureBuildSizesInfoKHR total{};

		for (const auto& accelerationStructure : accelerationStructures)
		{
			total.accelerationStructureSize += accelerationStructure.BuildSizes().accelerationStructureSize;
			total.buildScratchSize += accelerationStructure.BuildSizes().buildScratchSize;
			total.updateScratchSize += accelerationStructure.BuildSizes().updateScratchSize;
		}

		return total;
	}
}

Application::Application(const WindowConfig& windowConfig, const VkPresentModeKHR presentMode, const bool enableValidationLayers) :
	Vulkan::Application(windowConfig, presentMode, enableValidationLayers)
{

}

Application::~Application()
{
	Application::DeleteSwapChain();
	DeleteAccelerationStructures();
	DeleteProbeTextureImage();

	rayTracingProperties_.reset();
	deviceProcedures_.reset();
}

void Application::SetPhysicalDevice(
	VkPhysicalDevice physicalDevice,
	std::vector<const char*>& requiredExtensions,
	VkPhysicalDeviceFeatures& deviceFeatures,
	void* nextDeviceFeatures)
{
	// Required extensions.
	requiredExtensions.insert(requiredExtensions.end(),
	{	
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
	});

	// Required device features.
	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
	bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.pNext = nextDeviceFeatures;
	bufferDeviceAddressFeatures.bufferDeviceAddress = true;

	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	indexingFeatures.pNext = &bufferDeviceAddressFeatures;
	indexingFeatures.runtimeDescriptorArray = true;
	indexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.pNext = &indexingFeatures;
	accelerationStructureFeatures.accelerationStructure = true;
	
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures = {};
	rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingFeatures.pNext = &accelerationStructureFeatures;
	rayTracingFeatures.rayTracingPipeline = true;

	Vulkan::Application::SetPhysicalDevice(physicalDevice, requiredExtensions, deviceFeatures, &rayTracingFeatures);
}

void Application::OnDeviceSet()
{
	Vulkan::Application::OnDeviceSet();

	deviceProcedures_.reset(new DeviceProcedures(Device()));
	rayTracingProperties_.reset(new RayTracingProperties(Device()));
}

 void Application::CreateAccelerationStructures()
{
	const auto timer = std::chrono::high_resolution_clock::now();

	SingleTimeCommands::Submit(CommandPool(), [this](VkCommandBuffer commandBuffer)
	{
		CreateBottomLevelStructures(commandBuffer);
		CreateTopLevelStructures(commandBuffer);
	} );

	topScratchBuffer_.reset();
	topScratchBufferMemory_.reset();
	bottomScratchBuffer_.reset();
	bottomScratchBufferMemory_.reset();

	const auto elapsed = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - timer).count();
	std::cout << "- built acceleration structures in " << elapsed << "s" << std::endl;
}

void Application::DeleteAccelerationStructures()
{
	topAs_.clear();
	instancesBuffer_.reset();
	instancesBufferMemory_.reset();
	topScratchBuffer_.reset();
	topScratchBufferMemory_.reset();
	topBuffer_.reset();
	topBufferMemory_.reset();

	bottomAs_.clear();
	bottomScratchBuffer_.reset();
	bottomScratchBufferMemory_.reset();
	bottomBuffer_.reset();
	bottomBufferMemory_.reset();
	
}

void Application::CreateSwapChain()
{
	Vulkan::Application::CreateSwapChain();

	CreateOutputImage();



	rayTracingPipeline_.reset(new RayTracingPipeline(*deviceProcedures_, SwapChain(), topAs_[0], *accumulationImageView_, *outputImageView_, UniformBuffers(), GetScene(),lightProbes,lightProbePosBuffer));

	const std::vector<ShaderBindingTable::Entry> rayGenPrograms = { {rayTracingPipeline_->RayGenShaderIndex(), {}} };
	const std::vector<ShaderBindingTable::Entry> missPrograms = { {rayTracingPipeline_->MissShaderIndex(), {}} };
	const std::vector<ShaderBindingTable::Entry> hitGroups = { {rayTracingPipeline_->TriangleHitGroupIndex(), {}}, {rayTracingPipeline_->ProceduralHitGroupIndex(), {}} };

	shaderBindingTable_.reset(new ShaderBindingTable(*deviceProcedures_, *rayTracingPipeline_, *rayTracingProperties_, rayGenPrograms, missPrograms, hitGroups));



	lightProbeRTPipeline.reset(new LightProbeRTPipeline(*deviceProcedures_, SwapChain(), topAs_[0], *accumulationImageView_, *outputImageView_, UniformBuffers(), GetScene(), lightProbes, lightProbePosBuffer));

	const std::vector<ShaderBindingTable::Entry> rayLPGenPrograms = { {lightProbeRTPipeline->RayGenShaderIndex(), {}} };
	const std::vector<ShaderBindingTable::Entry> missLPPrograms = { {lightProbeRTPipeline->MissShaderIndex(), {}} };
	const std::vector<ShaderBindingTable::Entry> hitLPGroups = { {lightProbeRTPipeline->TriangleHitGroupIndex(), {}}, {lightProbeRTPipeline->ProceduralHitGroupIndex(), {}} };

	lightProbeShaderBindingTable_.reset(new ShaderBindingTable(*deviceProcedures_, *lightProbeRTPipeline, *rayTracingProperties_, rayLPGenPrograms, missLPPrograms, hitLPGroups));
}

void Application::DeleteSwapChain()
{
	shaderBindingTable_.reset();
	rayTracingPipeline_.reset();

	lightProbeShaderBindingTable_.reset();
	lightProbeRTPipeline.reset();

	outputImageView_.reset();
	outputImage_.reset();
	outputImageMemory_.reset();
	accumulationImageView_.reset();
	accumulationImage_.reset();
	accumulationImageMemory_.reset();

	Vulkan::Application::DeleteSwapChain();
}

void Application::Render(VkCommandBuffer commandBuffer, const uint32_t imageIndex)
{
	const auto extent = SwapChain().Extent();


	if (!isPrecomputed)
	{
		Render_LightProbe(commandBuffer, imageIndex);
		isPrecomputed = true;
	}

	VkDescriptorSet descriptorSets[] = { rayTracingPipeline_->DescriptorSet(imageIndex) };

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	// Acquire destination images for rendering.
	ImageMemoryBarrier::Insert(commandBuffer, accumulationImage_->Handle(), subresourceRange, 0,
		VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	ImageMemoryBarrier::Insert(commandBuffer, outputImage_->Handle(), subresourceRange, 0,
		VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);


	// Bind ray tracing pipeline.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline_->Handle());
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline_->PipelineLayout().Handle(), 0, 1, descriptorSets, 0, nullptr);

	// Describe the shader binding table.
	VkStridedDeviceAddressRegionKHR raygenShaderBindingTable = {};
	raygenShaderBindingTable.deviceAddress = shaderBindingTable_->RayGenDeviceAddress();
	raygenShaderBindingTable.stride = shaderBindingTable_->RayGenEntrySize();
	raygenShaderBindingTable.size = shaderBindingTable_->RayGenSize();

	VkStridedDeviceAddressRegionKHR missShaderBindingTable = {};
	missShaderBindingTable.deviceAddress = shaderBindingTable_->MissDeviceAddress();
	missShaderBindingTable.stride = shaderBindingTable_->MissEntrySize();
	missShaderBindingTable.size = shaderBindingTable_->MissSize();

	VkStridedDeviceAddressRegionKHR hitShaderBindingTable = {};
	hitShaderBindingTable.deviceAddress = shaderBindingTable_->HitGroupDeviceAddress();
	hitShaderBindingTable.stride = shaderBindingTable_->HitGroupEntrySize();
	hitShaderBindingTable.size = shaderBindingTable_->HitGroupSize();	

	VkStridedDeviceAddressRegionKHR callableShaderBindingTable = {};

	uint32_t numOfProbes = lightProbes.size();
	uint32_t isRaytrace = ShowOriginalRaytrace ? 1 : 0;
	uint32_t isShowTexture = 0;
	if (isRaytrace == 0)
	{
		isShowTexture = ShowLightProbeTexture ? 1 : 0;
	}

	vkCmdPushConstants(commandBuffer, rayTracingPipeline_->PipelineLayout().Handle(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t), &numOfProbes);
	vkCmdPushConstants(commandBuffer, rayTracingPipeline_->PipelineLayout().Handle(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, sizeof(uint32_t), sizeof(uint32_t), &isShowTexture);
	vkCmdPushConstants(commandBuffer, rayTracingPipeline_->PipelineLayout().Handle(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2* sizeof(uint32_t), sizeof(uint32_t), &isRaytrace);
	vkCmdPushConstants(commandBuffer, rayTracingPipeline_->PipelineLayout().Handle(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3* sizeof(uint32_t), sizeof(uint32_t), &currentProbeIndex);

	// Execute ray tracing shaders.
	deviceProcedures_->vkCmdTraceRaysKHR(commandBuffer,
		&raygenShaderBindingTable, &missShaderBindingTable, &hitShaderBindingTable, &callableShaderBindingTable,
		extent.width, extent.height, 1);

	// Acquire output image and swap-chain image for copying.
	ImageMemoryBarrier::Insert(commandBuffer, outputImage_->Handle(), subresourceRange, 
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	ImageMemoryBarrier::Insert(commandBuffer, SwapChain().Images()[imageIndex], subresourceRange, 0,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy output image into swap-chain image.
	VkImageCopy copyRegion;
	copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	copyRegion.srcOffset = { 0, 0, 0 };
	copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	copyRegion.dstOffset = { 0, 0, 0 };
	copyRegion.extent = { extent.width, extent.height, 1 };

	vkCmdCopyImage(commandBuffer,
		outputImage_->Handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		SwapChain().Images()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copyRegion);

	ImageMemoryBarrier::Insert(commandBuffer, SwapChain().Images()[imageIndex], subresourceRange, VK_ACCESS_TRANSFER_WRITE_BIT,
		0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);


	 
	//VkImageSubresourceRange subresourceRange = {};
	//subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//subresourceRange.baseMipLevel = 0;
	//subresourceRange.levelCount = 1;
	//subresourceRange.baseArrayLayer = 0;
	//subresourceRange.layerCount = 1;

	//ImageMemoryBarrier::Insert(commandBuffer, SwapChain().Images()[imageIndex], subresourceRange, 0,
	//	VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//// Copy output image into swap-chain image.
	//VkImageCopy copyRegion;
	//copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	//copyRegion.srcOffset = { 0, 0, 0 };
	//copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	//copyRegion.dstOffset = { 0, 0, 0 };
	//copyRegion.extent = { 1024, 1024, 1 };

	//vkCmdCopyImage(commandBuffer,
	//	lightProbes[0].radianceDistribution->probeImage->Handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//	SwapChain().Images()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	1, &copyRegion);

	//ImageMemoryBarrier::Insert(commandBuffer, SwapChain().Images()[imageIndex], subresourceRange, VK_ACCESS_TRANSFER_WRITE_BIT,
	//	0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

}

void Application::Render_LightProbe(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{


	const auto extent = SwapChain().Extent();

	VkDescriptorSet descriptorSets[] = { lightProbeRTPipeline->DescriptorSet(imageIndex) };

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;


	// Bind ray tracing pipeline.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lightProbeRTPipeline->Handle());
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, lightProbeRTPipeline->PipelineLayout().Handle(), 0, 1, descriptorSets, 0, nullptr);

	// Describe the shader binding table.
	VkStridedDeviceAddressRegionKHR raygenShaderBindingTable = {};
	raygenShaderBindingTable.deviceAddress = lightProbeShaderBindingTable_->RayGenDeviceAddress();
	raygenShaderBindingTable.stride = lightProbeShaderBindingTable_->RayGenEntrySize();
	raygenShaderBindingTable.size = lightProbeShaderBindingTable_->RayGenSize();

	VkStridedDeviceAddressRegionKHR missShaderBindingTable = {};
	missShaderBindingTable.deviceAddress = lightProbeShaderBindingTable_->MissDeviceAddress();
	missShaderBindingTable.stride = lightProbeShaderBindingTable_->MissEntrySize();
	missShaderBindingTable.size = lightProbeShaderBindingTable_->MissSize();

	VkStridedDeviceAddressRegionKHR hitShaderBindingTable = {};
	hitShaderBindingTable.deviceAddress = lightProbeShaderBindingTable_->HitGroupDeviceAddress();
	hitShaderBindingTable.stride = lightProbeShaderBindingTable_->HitGroupEntrySize();
	hitShaderBindingTable.size = lightProbeShaderBindingTable_->HitGroupSize();

	VkStridedDeviceAddressRegionKHR callableShaderBindingTable = {};

	// Execute ray tracing shaders.

	for (int i = 0; i < lightProbes.size(); i++)
	{


		uint32_t index = i;
		vkCmdPushConstants(commandBuffer, lightProbeRTPipeline->PipelineLayout().Handle(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t), &index);

		deviceProcedures_->vkCmdTraceRaysKHR(commandBuffer,
			&raygenShaderBindingTable, &missShaderBindingTable, &hitShaderBindingTable, &callableShaderBindingTable,
			1024, 1024, 1);

		//ImageMemoryBarrier::Insert(
		//	commandBuffer,
		//	lightProbes[i].radianceDistribution->probeImage->Handle(),
		//	subresourceRange,
		//	VK_ACCESS_SHADER_WRITE_BIT, // srcAccessMask: No accesses need to be completed before the barrier
		//	VK_ACCESS_SHADER_READ_BIT, // dstAccessMask: Shader writes must wait for the barrier
		//	VK_IMAGE_LAYOUT_GENERAL, // oldLayout: The initial layout is undefined
		//	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL // newLayout: We want to transition to the general layout
		//);
	}

}

void Application::CreateBottomLevelStructures(VkCommandBuffer commandBuffer)
{
	const auto& scene = GetScene();
	const auto& debugUtils = Device().DebugUtils();
	
	// Bottom level acceleration structure
	// Triangles via vertex buffers. Procedurals via AABBs.
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t aabbOffset = 0;

	for (const auto& model : scene.Models())
	{
		const auto vertexCount = static_cast<uint32_t>(model.NumberOfVertices());
		const auto indexCount = static_cast<uint32_t>(model.NumberOfIndices());
		BottomLevelGeometry geometries;
		
		model.Procedural()
			? geometries.AddGeometryAabb(scene, aabbOffset, 1, true)
			: geometries.AddGeometryTriangles(scene, vertexOffset, vertexCount, indexOffset, indexCount, true);

		bottomAs_.emplace_back(*deviceProcedures_, *rayTracingProperties_, geometries);

		vertexOffset += vertexCount * sizeof(Assets::Vertex);
		indexOffset += indexCount * sizeof(uint32_t);
		aabbOffset += sizeof(VkAabbPositionsKHR);
	}

	// Allocate the structures memory.
	const auto total = GetTotalRequirements(bottomAs_);

	bottomBuffer_.reset(new Buffer(Device(), total.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
	bottomBufferMemory_.reset(new DeviceMemory(bottomBuffer_->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
	bottomScratchBuffer_.reset(new Buffer(Device(), total.buildScratchSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	bottomScratchBufferMemory_.reset(new DeviceMemory(bottomScratchBuffer_->AllocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));

	debugUtils.SetObjectName(bottomBuffer_->Handle(), "BLAS Buffer");
	debugUtils.SetObjectName(bottomBufferMemory_->Handle(), "BLAS Memory");
	debugUtils.SetObjectName(bottomScratchBuffer_->Handle(), "BLAS Scratch Buffer");
	debugUtils.SetObjectName(bottomScratchBufferMemory_->Handle(), "BLAS Scratch Memory");

	// Generate the structures.
	VkDeviceSize resultOffset = 0;
	VkDeviceSize scratchOffset = 0;

	for (size_t i = 0; i != bottomAs_.size(); ++i)
	{
		bottomAs_[i].Generate(commandBuffer, *bottomScratchBuffer_, scratchOffset, *bottomBuffer_, resultOffset);
		
		resultOffset += bottomAs_[i].BuildSizes().accelerationStructureSize;
		scratchOffset += bottomAs_[i].BuildSizes().buildScratchSize;

		debugUtils.SetObjectName(bottomAs_[i].Handle(), ("BLAS #" + std::to_string(i)).c_str());
	}
}

void Application::CreateTopLevelStructures(VkCommandBuffer commandBuffer)
{
	const auto& scene = GetScene();
	const auto& debugUtils = Device().DebugUtils();

	// Top level acceleration structure
	std::vector<VkAccelerationStructureInstanceKHR> instances;

	// Hit group 0: triangles
	// Hit group 1: procedurals
	uint32_t instanceId = 0;

	for (const auto& model : scene.Models())
	{
		instances.push_back(
			TopLevelAccelerationStructure::CreateInstance(bottomAs_[instanceId], glm::mat4(1), instanceId, model.Procedural() ? 1 : 0)
		);

		instanceId++;
	}

	// Create and copy instances buffer (do it in a separate one-time synchronous command buffer).
	BufferUtil::CreateDeviceBuffer(CommandPool(), "TLAS Instances", VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, instances, instancesBuffer_, instancesBufferMemory_);

	// Memory barrier for the bottom level acceleration structure builds.
	AccelerationStructure::MemoryBarrier(commandBuffer);
	
	topAs_.emplace_back(
		*deviceProcedures_, 
		*rayTracingProperties_, 
		instancesBuffer_->GetDeviceAddress(), 
		static_cast<uint32_t>(instances.size())
	);

	// Allocate the structure memory.
	const auto total = GetTotalRequirements(topAs_);

	topBuffer_.reset(new Buffer(Device(), total.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
	topBufferMemory_.reset(new DeviceMemory(topBuffer_->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));

	topScratchBuffer_.reset(new Buffer(Device(), total.buildScratchSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	topScratchBufferMemory_.reset(new DeviceMemory(topScratchBuffer_->AllocateMemory(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));

	
	debugUtils.SetObjectName(topBuffer_->Handle(), "TLAS Buffer");
	debugUtils.SetObjectName(topBufferMemory_->Handle(), "TLAS Memory");
	debugUtils.SetObjectName(topScratchBuffer_->Handle(), "TLAS Scratch Buffer");
	debugUtils.SetObjectName(topScratchBufferMemory_->Handle(), "TLAS Scratch Memory");
	debugUtils.SetObjectName(instancesBuffer_->Handle(), "TLAS Instances Buffer");
	debugUtils.SetObjectName(instancesBufferMemory_->Handle(), "TLAS Instances Memory");

	// Generate the structures.
	topAs_[0].Generate(commandBuffer, *topScratchBuffer_, 0, *topBuffer_, 0);


	debugUtils.SetObjectName(topAs_[0].Handle(), "TLAS");

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	if (!isLightProbeCreated)
	{
		CreateProbeTextureImage();

		for (int i = 0; i < lightProbes.size(); i++)
		{
			// Insert a memory barrier to transition the image layout to VK_IMAGE_LAYOUT_GENERAL
			ImageMemoryBarrier::Insert(
				commandBuffer,
				lightProbes[i].radianceDistribution->probeImage->Handle(),
				subresourceRange,
				0, // srcAccessMask: No accesses need to be completed before the barrier
				VK_ACCESS_SHADER_WRITE_BIT, // dstAccessMask: Shader writes must wait for the barrier
				VK_IMAGE_LAYOUT_UNDEFINED, // oldLayout: The initial layout is undefined
				VK_IMAGE_LAYOUT_GENERAL // newLayout: We want to transition to the general layout
			);
		}
		isLightProbeCreated = true;
	}
}

void Application::CreateOutputImage()
{
	const auto extent = SwapChain().Extent();
	const auto format = SwapChain().Format();
	const auto tiling = VK_IMAGE_TILING_OPTIMAL;

	accumulationImage_.reset(new Image(Device(), extent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT));
	accumulationImageMemory_.reset(new DeviceMemory(accumulationImage_->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
	accumulationImageView_.reset(new ImageView(Device(), accumulationImage_->Handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT));

	outputImage_.reset(new Image(Device(), extent, format, tiling, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
	outputImageMemory_.reset(new DeviceMemory(outputImage_->AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)));
	outputImageView_.reset(new ImageView(Device(), outputImage_->Handle(), format, VK_IMAGE_ASPECT_COLOR_BIT));

	const auto& debugUtils = Device().DebugUtils();
	
	debugUtils.SetObjectName(accumulationImage_->Handle(), "Accumulation Image");
	debugUtils.SetObjectName(accumulationImageMemory_->Handle(), "Accumulation Image Memory");
	debugUtils.SetObjectName(accumulationImageView_->Handle(), "Accumulation ImageView");
	
	debugUtils.SetObjectName(outputImage_->Handle(), "Output Image");
	debugUtils.SetObjectName(outputImageMemory_->Handle(), "Output Image Memory");
	debugUtils.SetObjectName(outputImageView_->Handle(), "Output ImageView");

}

void Application::CreateProbeTextureImage()
{

	//TODO: automatic light probe placement

	int probeNum = 1000; // Total number of probes
	const auto& debugUtils = Device().DebugUtils();

	lightProbes.reserve(probeNum);
	lightProbePos.reserve(probeNum);

	// Define the side length and the number of probes per edge
	float sideLength = 4.5f;
	int probesPerEdge = 4;

	// Define the midpoints of the planes
	std::vector<glm::vec3> midpoints = {
		glm::vec3(0, 2.5, 0),
		glm::vec3(0, 0, 0),
		glm::vec3(0, -2.5, 0)
	};

	// Loop through each plane
	for (const auto& midpoint : midpoints) {
		// Place probes at the vertices and edge midpoints of the square
		for (int i = 0; i <= probesPerEdge; ++i) {
			for (int j = 0; j <= probesPerEdge; ++j) {
				float x = midpoint.x + (i - probesPerEdge/2) * sideLength / 2;
				float y = midpoint.y;
				float z = midpoint.z + (j - probesPerEdge/2) * sideLength / 2;

				// Skip the center point of the middle plane
				if ((x == 0.0f) && (y == 0.0f) && (z == 0.0f)) {
					continue;
				}

				// Skip the center point, as we only want vertices and edge midpoints
				lightProbes.emplace_back(glm::vec3(x, y, z), Device());
			}
		}
	}
	
	//lightProbes.emplace_back(glm::vec3(0, 0, -3.5), Device());
	//lightProbes.emplace_back(glm::vec3(0, 0, -2.5), Device());

	numOfProbe = lightProbes.size();

	for (int i = 0; i < lightProbes.size(); i++) {

		lightProbePos.emplace_back(glm::vec4(lightProbes[i].position, 0.0f));

		debugUtils.SetObjectName(lightProbes[i].radianceDistribution->probeImage->Handle(), i + " : radianceDistribution Image");
		debugUtils.SetObjectName(lightProbes[i].radianceDistribution->probeImageMemory->Handle(), i + " : radianceDistribution probeImageMemory");
		debugUtils.SetObjectName(lightProbes[i].radianceDistribution->probeImageView->Handle(), i + " : radianceDistribution probeImageView");
	}


	Vulkan::BufferUtil::CreateDeviceBuffer(CommandPool(), "lightProbePos", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, lightProbePos, lightProbePosBuffer, lightProbePosBufferMemory);
	
}

void Application::DeleteProbeTextureImage()

{
	lightProbePos.clear();
	lightProbePosBuffer.reset();
	lightProbePosBufferMemory.reset();
	lightProbes.clear();
}

}
