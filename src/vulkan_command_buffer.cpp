#include "vulkan_command_buffer.hpp"

#include <stdexcept>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "vulkan_buffer.hpp"
#include "vulkan_context.hpp"
#include "vulkan_descriptors.hpp"
#include "vulkan_device.hpp"
#include "vulkan_sync.hpp"
#include "vulkan_framebuffer.hpp"
#include "vulkan_image.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_queues.hpp"
#include "vulkan_render_pass.hpp"
#include "utils/logger.hpp"
#include "utils/vulkan_base.hpp"

std::map<VkImageLayout, VulkanMemoryBarrierBuilder::AccessData> VulkanMemoryBarrierBuilder::s_TransitionMapping = {
    {VK_IMAGE_LAYOUT_UNDEFINED,
        {VK_ACCESS_NONE, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT}},
    {VK_IMAGE_LAYOUT_GENERAL,
        {VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT}},
    {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}},
    {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT}},
    {VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        {VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        {VK_ACCESS_TRANSFER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_ACCESS_TRANSFER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT}},
    {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        {VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT}},
    {VK_IMAGE_LAYOUT_PREINITIALIZED,
        {VK_ACCESS_HOST_WRITE_BIT,
         VK_PIPELINE_STAGE_HOST_BIT,
         VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT}},
#if defined(VK_VERSION_1_1)
    {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
#endif
#if defined(VK_VERSION_1_2)
    {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT}},
    {VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT}},
    {VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
        {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
#endif
#if defined(VK_VERSION_1_3)
    {VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        {VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
    {VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT}},
#endif
#if defined(VK_VERSION_1_4)
    {VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
        {VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT}},
#endif
    {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        {VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
         VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT}},
    {VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
        {VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT}},
    {VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
        {VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
         VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
         VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
         VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT}},
    {VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
        {VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
         VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
         VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV,
         VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR}}
};

VulkanMemoryBarrierBuilder::VulkanMemoryBarrierBuilder(const ResourceID p_Device, const VkPipelineStageFlags p_SrcStageMask, const VkPipelineStageFlags p_DstStageMask, const VkDependencyFlags p_DependencyFlags)
    : m_Device(p_Device), m_SrcStageMask(p_SrcStageMask), m_DstStageMask(p_DstStageMask), m_DependencyFlags(p_DependencyFlags) {}

void VulkanMemoryBarrierBuilder::addAbsoluteMemoryBarrier()
{
    VkMemoryBarrier l_Barrier{};
    l_Barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    l_Barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    l_Barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    m_MemoryBarriers.push_back(l_Barrier);
}

void VulkanMemoryBarrierBuilder::addMemoryBarrier(const VkAccessFlags p_SrcAccessMask, const VkAccessFlags p_DstAccessMask)
{
    VkMemoryBarrier l_Barrier{};
    l_Barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    l_Barrier.srcAccessMask = p_SrcAccessMask;
    l_Barrier.dstAccessMask = p_DstAccessMask;
    m_MemoryBarriers.push_back(l_Barrier);
}

void VulkanMemoryBarrierBuilder::addBufferMemoryBarrier(const ResourceID p_Buffer, const VkDeviceSize p_Offset, const VkDeviceSize p_Size, const VkAccessFlags p_SrcAccessMask, const VkAccessFlags p_DstAccessMask, const uint32_t p_DstQueueFamily)
{
    const VulkanBuffer& l_Buffer = VulkanContext::getDevice(m_Device).getBuffer(p_Buffer);

    VkBufferMemoryBarrier l_Barrier{};
    l_Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    l_Barrier.srcAccessMask = p_SrcAccessMask;
    l_Barrier.dstAccessMask = p_DstAccessMask;
    l_Barrier.buffer = *l_Buffer;
    l_Barrier.offset = p_Offset;
    l_Barrier.size = p_Size;
    if (p_DstQueueFamily != VK_QUEUE_FAMILY_IGNORED && l_Buffer.m_QueueFamilyIndex != p_DstQueueFamily)
    {
        l_Barrier.srcQueueFamilyIndex = l_Buffer.m_QueueFamilyIndex;
        l_Barrier.dstQueueFamilyIndex = p_DstQueueFamily;
    }
    else
    {
        l_Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        l_Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
    m_BufferMemoryBarriers.push_back(l_Barrier);
}

void VulkanMemoryBarrierBuilder::addImageMemoryBarrier(const ResourceID p_Image, const VkImageLayout p_NewLayout, const uint32_t p_DstQueueFamily, const VkAccessFlags p_SrcAccessMask, const VkAccessFlags p_DstAccessMask)
{
    const VulkanImage& l_Image = VulkanContext::getDevice(m_Device).getImage(p_Image);
    addImageMemoryBarrier(l_Image, p_NewLayout, p_DstQueueFamily, p_SrcAccessMask, p_DstAccessMask);
}

void VulkanMemoryBarrierBuilder::addImageMemoryBarrier(const VulkanImage& p_Image, const VkImageLayout p_NewLayout, const uint32_t p_DstQueueFamily, const VkAccessFlags p_SrcAccessMask, const VkAccessFlags p_DstAccessMask)
{
    VkImageMemoryBarrier l_Barrier{};
    l_Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    l_Barrier.srcAccessMask = p_SrcAccessMask == VK_ACCESS_FLAG_BITS_MAX_ENUM ? s_TransitionMapping[p_Image.getLayout()].srcAccessMask : p_SrcAccessMask;
    l_Barrier.dstAccessMask = p_DstAccessMask == VK_ACCESS_FLAG_BITS_MAX_ENUM ? s_TransitionMapping[p_NewLayout].dstAccessMask : p_DstAccessMask;
    l_Barrier.oldLayout = p_Image.getLayout();
    l_Barrier.newLayout = p_NewLayout;
    if (p_DstQueueFamily != VK_QUEUE_FAMILY_IGNORED && p_Image.m_QueueFamilyIndex != p_DstQueueFamily)
    {
        l_Barrier.srcQueueFamilyIndex = p_Image.m_QueueFamilyIndex;
        l_Barrier.dstQueueFamilyIndex = p_DstQueueFamily;
    }
    else
    {
        l_Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        l_Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
    l_Barrier.image = *p_Image;
    l_Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    l_Barrier.subresourceRange.baseMipLevel = 0;
    l_Barrier.subresourceRange.levelCount = 1;
    l_Barrier.subresourceRange.baseArrayLayer = 0;
    l_Barrier.subresourceRange.layerCount = 1;
    m_ImageMemoryBarriers.push_back(l_Barrier);
}

void VulkanCommandBuffer::beginRecording(const VkCommandBufferUsageFlags p_Flags)
{
    if (m_IsRecording)
    {
        LOG_WARN("Tried to begin recording, but command buffer (ID:", m_ID, ") is already recording");
        return;
    }

    VkCommandBufferBeginInfo l_BeginInfo{};
    l_BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    l_BeginInfo.flags = p_Flags;

    VulkanContext::getDevice(getDeviceID()).getTable().vkBeginCommandBuffer(m_VkHandle, &l_BeginInfo);

    m_IsRecording = true;
}

void VulkanCommandBuffer::endRecording()
{
    if (!m_IsRecording)
    {
        LOG_WARN("Tried to end recording, but command buffer (ID:", m_ID, ") is not recording");
        return;
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkEndCommandBuffer(m_VkHandle);

    m_IsRecording = false;
    m_HasRecorded = true;
}

void VulkanCommandBuffer::cmdCopyBuffer(const ResourceID p_Source, const ResourceID p_Destination, const std::span<const VkBufferCopy> p_CopyRegions) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    l_Device.getTable().vkCmdCopyBuffer(m_VkHandle, *l_Device.getBuffer(p_Source), *l_Device.getBuffer(p_Destination), static_cast<uint32_t>(p_CopyRegions.size()), p_CopyRegions.data());
}

void VulkanCommandBuffer::cmdCopyBufferToImage(const ResourceID p_Buffer, const ResourceID p_Image, const VkImageLayout p_ImageLayout, const std::span<const VkBufferImageCopy> p_CopyRegions) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    l_Device.getTable().vkCmdCopyBufferToImage(m_VkHandle, *l_Device.getBuffer(p_Buffer), *l_Device.getImage(p_Image), p_ImageLayout, static_cast<uint32_t>(p_CopyRegions.size()), p_CopyRegions.data());
}

void VulkanCommandBuffer::cmdBlitImage(const ResourceID p_Source, const ResourceID p_Destination, const std::span<const VkImageBlit> p_Regions, const VkFilter p_Filter) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanImage& l_SrcImage = l_Device.getImage(p_Source);
    const VulkanImage& l_DstImage = l_Device.getImage(p_Destination);
    cmdBlitImage(l_SrcImage, l_DstImage, p_Regions, p_Filter);
}

void VulkanCommandBuffer::cmdBlitImage(const VulkanImage& p_Source, const VulkanImage& p_Destination, const std::span<const VkImageBlit> p_Regions, const VkFilter p_Filter) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    l_Device.getTable().vkCmdBlitImage(m_VkHandle, *p_Source, p_Source.getLayout(), *p_Destination, p_Destination.getLayout(), static_cast<uint32_t>(p_Regions.size()), p_Regions.data(), p_Filter);
}

void VulkanCommandBuffer::cmdSimpleBlitImage(const ResourceID p_Source, const ResourceID p_Destination, const VkFilter p_Filter) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanImage& l_SrcImage = l_Device.getImage(p_Source);
    const VulkanImage& l_DstImage = l_Device.getImage(p_Destination);
    cmdSimpleBlitImage(l_SrcImage, l_DstImage, p_Filter);
}

void VulkanCommandBuffer::cmdSimpleBlitImage(const VulkanImage& p_Source, const VulkanImage& p_Destination, const VkFilter p_Filter) const
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VkImageBlit l_Region{};
    l_Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    l_Region.srcSubresource.layerCount = 1;
    l_Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    l_Region.dstSubresource.layerCount = 1;
    l_Region.srcOffsets[1] = {static_cast<int32_t>(p_Source.getSize().width), static_cast<int32_t>(p_Source.getSize().height), 1};
    l_Region.dstOffsets[1] = {static_cast<int32_t>(p_Destination.getSize().width), static_cast<int32_t>(p_Destination.getSize().height), 1};

    //If source image layout is not transfer source, then we need to transition it
    if (p_Source.getLayout() != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        VkImageMemoryBarrier l_Rarrier{};
        l_Rarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        l_Rarrier.oldLayout = p_Source.getLayout();
        l_Rarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        l_Rarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        l_Rarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        l_Rarrier.image = *p_Source;
        l_Rarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        l_Rarrier.subresourceRange.baseMipLevel = 0;
        l_Rarrier.subresourceRange.levelCount = 1;
        l_Rarrier.subresourceRange.baseArrayLayer = 0;
        l_Rarrier.subresourceRange.layerCount = 1;
        l_Rarrier.srcAccessMask = 0;
        l_Rarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        l_Device.getTable().vkCmdPipelineBarrier(m_VkHandle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &l_Rarrier);
    }

    l_Device.getTable().vkCmdBlitImage(m_VkHandle, *p_Source, p_Source.getLayout(), *p_Destination, p_Destination.getLayout(), 1, &l_Region, p_Filter);
}

void VulkanCommandBuffer::ecmdDumpStagingBuffer(const ResourceID p_Buffer, const VkDeviceSize p_Size, const VkDeviceSize p_Offset) const
{
    std::array<VkBufferCopy, 1> l_Regions = {{{.srcOffset = 0, .dstOffset = p_Offset, .size = p_Size}}};
    ecmdDumpStagingBuffer(p_Buffer, l_Regions);
}

void VulkanCommandBuffer::ecmdDumpStagingBuffer(ResourceID p_Buffer, const std::span<const VkBufferCopy> p_Regions) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const ResourceID l_StagingBufferID = l_Device.getStagingBufferData().stagingBuffer;
    VulkanBuffer& l_StagingBuffer = l_Device.getBuffer(l_StagingBufferID);
    if (l_StagingBuffer.m_VkHandle == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Tried to dump staging buffer (ID: " + std::to_string(p_Buffer) + ") data, but staging buffer is not configured");
    }

    if (l_StagingBuffer.isMemoryMapped())
    {
        LOG_DEBUG("Automatically unmapping staging buffer before dumping into buffer (ID: ", p_Buffer, ")");
        l_StagingBuffer.unmap();
    }

    cmdCopyBuffer(l_StagingBufferID, p_Buffer, p_Regions);
}

void VulkanCommandBuffer::ecmdDumpStagingBufferToImage(const ResourceID p_Image, const VkExtent3D p_Size, const VkOffset3D p_Offset, const bool p_KeepLayout) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkBufferImageCopy l_Region;
    l_Region.bufferOffset = 0;
    l_Region.bufferRowLength = 0;
    l_Region.bufferImageHeight = 0;
    l_Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    l_Region.imageSubresource.mipLevel = 0;
    l_Region.imageSubresource.baseArrayLayer = 0;
    l_Region.imageSubresource.layerCount = 1;
    l_Region.imageOffset = p_Offset;
    l_Region.imageExtent = p_Size;

    const VkImageLayout l_Layout = l_Device.getImage(p_Image).getLayout();

    if (l_Layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        VulkanMemoryBarrierBuilder l_BarrierBuilder{getDeviceID(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0};
        l_BarrierBuilder.addImageMemoryBarrier(p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        cmdPipelineBarrier(l_BarrierBuilder);
    }
    std::array<VkBufferImageCopy, 1> l_RegionArray = {l_Region};
    cmdCopyBufferToImage(l_Device.getStagingBufferData().stagingBuffer, p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, l_RegionArray);
    if (l_Layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && p_KeepLayout)
    {
        VulkanMemoryBarrierBuilder l_BarrierBuilder{getDeviceID(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0};
        l_BarrierBuilder.addImageMemoryBarrier(p_Image, l_Layout);
        cmdPipelineBarrier(l_BarrierBuilder);
    }
}

void VulkanCommandBuffer::ecmdDumpDataIntoBuffer(const ResourceID p_DestBuffer, const uint8_t* p_Data, const VkDeviceSize p_Size) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VkDeviceSize l_StagingBufferSize = l_Device.getBuffer(l_Device.getStagingBufferData().stagingBuffer).getSize();

    VkDeviceSize l_Offset = 0;
    while (l_Offset < p_Size)
    {
        const VkDeviceSize l_NextSize = std::min(l_StagingBufferSize, p_Size - l_Offset);
        void* l_StagePtr = l_Device.mapStagingBuffer(l_NextSize, 0);
        memcpy(l_StagePtr, p_Data + l_Offset, l_NextSize);
        ecmdDumpStagingBuffer(p_DestBuffer, l_NextSize, l_Offset);
        l_Offset += l_NextSize;
    }
}

void VulkanCommandBuffer::ecmdDumpDataIntoImage(const ResourceID p_DestImage, const uint8_t* p_Data, const VkExtent3D p_Extent, const uint32_t p_BytesPerPixel, const bool p_KeepLayout) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanDevice::StagingBufferInfo l_StagingBufferInfo = l_Device.getStagingBufferData();
    const VkDeviceSize l_InitStagingBufferSize = l_Device.getBuffer(l_StagingBufferInfo.stagingBuffer).getSize();
    VkDeviceSize l_StagingBufferSize = l_InitStagingBufferSize;
    if (l_StagingBufferSize < p_Extent.width * p_Extent.height * p_BytesPerPixel)
    {
        l_Device.freeStagingBuffer();
        l_Device.configureStagingBuffer(p_Extent.width * p_Extent.height * p_BytesPerPixel, l_StagingBufferInfo.queue);
        l_StagingBufferSize = l_Device.getBuffer(l_StagingBufferInfo.stagingBuffer).getSize();
    }

    const VkDeviceSize l_Size = std::min(l_StagingBufferSize, static_cast<VkDeviceSize>(p_Extent.width * p_Extent.height * p_BytesPerPixel));

    void* l_StagePtr = l_Device.mapStagingBuffer(l_Size, 0);
    memcpy(l_StagePtr, p_Data, l_Size);
    ecmdDumpStagingBufferToImage(p_DestImage, p_Extent, {0, 0, 0}, p_KeepLayout);
}

void VulkanCommandBuffer::cmdPushConstant(const ResourceID p_Layout, const VkShaderStageFlags p_StageFlags, const uint32_t p_Offset, const uint32_t p_Size, const void* p_Values) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    l_Device.getTable().vkCmdPushConstants(m_VkHandle, l_Device.getPipelineLayout(p_Layout).m_VkHandle, p_StageFlags, p_Offset, p_Size, p_Values);
}

void VulkanCommandBuffer::cmdBindDescriptorSet(const VkPipelineBindPoint p_BindPoint, const ResourceID p_Layout, const ResourceID p_DescriptorSet) const
{
    const VkPipelineLayout l_VkLayout = *VulkanContext::getDevice(getDeviceID()).getPipelineLayout(p_Layout);
    const VkDescriptorSet l_VkDescriptorSet = *VulkanContext::getDevice(getDeviceID()).getDescriptorSet(p_DescriptorSet);
    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdBindDescriptorSets(m_VkHandle, p_BindPoint, l_VkLayout, 0, 1, &l_VkDescriptorSet, 0, nullptr);
}

void VulkanCommandBuffer::submit(const VulkanQueue& p_Queue, const std::span<const WaitSemaphoreData> p_WaitSemaphoreData, const std::span<const ResourceID> p_SignalSemaphores, const ResourceID p_Fence)
{
    if (m_IsRecording)
    {
        LOG_WARN("Tried to submit command buffer (ID:", m_ID, ") while it is still recording, forcefully ending recording");
        endRecording();
    }

    if (!m_HasRecorded)
    {
        LOG_WARN("Tried to submit command buffer (ID:", m_ID, ") without recording any commands");
        return;
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    TRANS_VECTOR(l_WaitSemaphores, VkSemaphore);
    l_WaitSemaphores.resize(p_WaitSemaphoreData.size());
    TRANS_VECTOR(l_WaitStages, VkPipelineStageFlags);
    l_WaitStages.resize(p_WaitSemaphoreData.size());
    for (size_t i = 0; i < l_WaitSemaphores.size(); i++)
    {
        l_WaitSemaphores[i] = l_Device.getSemaphore(p_WaitSemaphoreData[i].semaphore).m_VkHandle;
        l_WaitStages[i] = p_WaitSemaphoreData[i].stages;
    }

    TRANS_VECTOR(l_SignalSemaphoresVk, VkSemaphore);
    l_SignalSemaphoresVk.reserve(p_SignalSemaphores.size());
    for (const ResourceID& l_Semaphore : p_SignalSemaphores)
    {
        l_SignalSemaphoresVk.push_back(l_Device.getSemaphore(l_Semaphore).m_VkHandle);
    }

    VkSubmitInfo l_SubmitInfo{};
    l_SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    l_SubmitInfo.commandBufferCount = 1;
    l_SubmitInfo.pCommandBuffers = &m_VkHandle;
    l_SubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(l_WaitSemaphores.size());
    l_SubmitInfo.pWaitSemaphores = l_WaitSemaphores.data();
    l_SubmitInfo.pWaitDstStageMask = l_WaitStages.data();
    l_SubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(l_SignalSemaphoresVk.size());
    l_SubmitInfo.pSignalSemaphores = l_SignalSemaphoresVk.data();

    VULKAN_TRY(l_Device.getTable().vkQueueSubmit(p_Queue.m_VkHandle, 1, &l_SubmitInfo, p_Fence != UINT32_MAX ? l_Device.getFence(p_Fence).m_VkHandle : VK_NULL_HANDLE));

    m_HasSubmitted = true;
}

void VulkanCommandBuffer::reset() const
{
    VULKAN_TRY(VulkanContext::getDevice(getDeviceID()).getTable().vkResetCommandBuffer(m_VkHandle, 0));
}

void VulkanCommandBuffer::cmdBeginRenderPass(const ResourceID p_RenderPass, const ResourceID p_FrameBuffer, const VkExtent2D p_Extent, const std::span<VkClearValue> p_ClearValues) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdBeginRenderPass, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VkRenderPassBeginInfo l_BeginInfo{};
    l_BeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    l_BeginInfo.renderPass = l_Device.getRenderPass(p_RenderPass).m_VkHandle;
    l_BeginInfo.framebuffer = l_Device.getFramebuffer(p_FrameBuffer).m_VkHandle;
    l_BeginInfo.renderArea.offset = {0, 0};
    l_BeginInfo.renderArea.extent = p_Extent;

    l_BeginInfo.clearValueCount = static_cast<uint32_t>(p_ClearValues.size());
    l_BeginInfo.pClearValues = p_ClearValues.data();

    l_Device.getTable().vkCmdBeginRenderPass(m_VkHandle, &l_BeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::cmdEndRenderPass() const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdEndRenderPass, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdEndRenderPass(m_VkHandle);
}

void VulkanCommandBuffer::cmdBindPipeline(const VkPipelineBindPoint p_BindPoint, const ResourceID p_Pipeline) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdBindPipeline, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    l_Device.getTable().vkCmdBindPipeline(m_VkHandle, p_BindPoint, l_Device.getPipeline(p_Pipeline).m_VkHandle);
}

void VulkanCommandBuffer::cmdNextSubpass() const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdNextSubpass, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdNextSubpass(m_VkHandle, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::cmdPipelineBarrier(const VulkanMemoryBarrierBuilder& p_Builder) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdPipelineBarrier, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

	VulkanContext::getDevice(getDeviceID()).getTable().vkCmdPipelineBarrier(m_VkHandle, 
        p_Builder.m_SrcStageMask, p_Builder.m_DstStageMask, p_Builder.m_DependencyFlags, 
        static_cast<uint32_t>(p_Builder.m_MemoryBarriers.size()), p_Builder.m_MemoryBarriers.data(), 
        static_cast<uint32_t>(p_Builder.m_BufferMemoryBarriers.size()), p_Builder.m_BufferMemoryBarriers.data(), 
        static_cast<uint32_t>(p_Builder.m_ImageMemoryBarriers.size()), p_Builder.m_ImageMemoryBarriers.data());
}

void VulkanCommandBuffer::cmdBindVertexBuffer(const ResourceID p_Buffer, const VkDeviceSize p_Offset) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdBindVertexBuffers, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdBindVertexBuffers(m_VkHandle, 0, 1, &VulkanContext::getDevice(getDeviceID()).getBuffer(p_Buffer).m_VkHandle, &p_Offset);
}

void VulkanCommandBuffer::cmdBindVertexBuffers(const std::span<const ResourceID> p_BufferIDs, const std::span<const VkDeviceSize> p_Offsets) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdBindVertexBuffers, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    TRANS_VECTOR(l_VkBuffers, VkBuffer);
    l_VkBuffers.reserve(p_BufferIDs.size());
    for (const ResourceID& l_Buffer : p_BufferIDs)
    {
        l_VkBuffers.push_back(l_Device.getBuffer(l_Buffer).m_VkHandle);
    }
    l_Device.getTable().vkCmdBindVertexBuffers(m_VkHandle, 0, static_cast<uint32_t>(l_VkBuffers.size()), l_VkBuffers.data(), p_Offsets.data());
}

void VulkanCommandBuffer::cmdBindIndexBuffer(const ResourceID p_BufferID, const VkDeviceSize p_Offset, const VkIndexType p_IndexType) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdBindIndexBuffer, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    l_Device.getTable().vkCmdBindIndexBuffer(m_VkHandle, l_Device.getBuffer(p_BufferID).m_VkHandle, p_Offset, p_IndexType);
}

void VulkanCommandBuffer::cmdSetViewport(const VkViewport& p_Viewport) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdSetViewport, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdSetViewport(m_VkHandle, 0, 1, &p_Viewport);
}

void VulkanCommandBuffer::cmdSetScissor(const VkRect2D p_Scissor) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdSetScissor, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdSetScissor(m_VkHandle, 0, 1, &p_Scissor);
}

void VulkanCommandBuffer::cmdDraw(const uint32_t p_VertexCount, const uint32_t p_FirstVertex, const uint32_t p_InstanceCount, const uint32_t p_FirstInstance) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdDraw, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdDraw(m_VkHandle, p_VertexCount, p_InstanceCount, p_FirstVertex, p_FirstInstance);
}

void VulkanCommandBuffer::cmdDrawIndexed(const uint32_t p_IndexCount, const uint32_t p_FirstIndex, const int32_t p_VertexOffset, const uint32_t p_InstanceCount, const uint32_t p_FirstInstance) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdDrawIndexed, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }
    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdDrawIndexed(m_VkHandle, p_IndexCount, p_InstanceCount, p_FirstIndex, p_VertexOffset, p_FirstInstance);
}

void VulkanCommandBuffer::cmdDispatch(const uint32_t p_GroupCountX, const uint32_t p_GroupCountY, const uint32_t p_GroupCountZ) const
{
    if (!m_IsRecording)
    {
        throw std::runtime_error("Tried to execute command CmdDispatch, but command buffer (ID:" + std::to_string(m_ID) + ") is not recording");
    }

    VulkanContext::getDevice(getDeviceID()).getTable().vkCmdDispatch(m_VkHandle, p_GroupCountX, p_GroupCountY, p_GroupCountZ);
}

VkCommandBuffer VulkanCommandBuffer::operator*() const
{
    return m_VkHandle;
}

void VulkanCommandBuffer::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
        l_Device.getTable().vkFreeCommandBuffers(l_Device.m_VkHandle, l_Device.getCommandPool(m_FamilyIndex, m_ThreadID, m_Flags), 1, &m_VkHandle);
        LOG_DEBUG("Freed command buffer (ID:", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

VulkanCommandBuffer::VulkanCommandBuffer(const uint32_t p_Device, const VkCommandBuffer p_CommandBuffer, const TypeFlags p_Flags, const uint32_t p_FamilyIndex, const uint32_t p_ThreadID)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_CommandBuffer), m_Flags(p_Flags), m_FamilyIndex(p_FamilyIndex), m_ThreadID(p_ThreadID) {}
