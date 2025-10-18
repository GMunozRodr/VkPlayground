#include "vulkan_device.hpp"

#include <ranges>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#include "vulkan_context.hpp"
#include "ext/vulkan_extension_management.hpp"
#include "utils/logger.hpp"
#include "vulkan_base.hpp"

VulkanQueue VulkanDevice::getQueue(const QueueSelection& p_QueueSelection) const
{
    VkQueue l_Queue;
    getTable().vkGetDeviceQueue(m_VkHandle, p_QueueSelection.familyIndex, p_QueueSelection.queueIndex, &l_Queue);
    return VulkanQueue(l_Queue);
}

VulkanDeviceSubresource* VulkanDevice::getSubresource(const ResourceID p_ID) const
{
    if (m_Subresources.contains(p_ID))
    {
        return m_Subresources.at(p_ID);
    }
    return nullptr;
}

bool VulkanDevice::freeSubresource(const ResourceID p_ID)
{
    VulkanDeviceSubresource* l_Component = getSubresource(p_ID);
    if (l_Component)
    {
        l_Component->free();
        m_Subresources.erase(p_ID);
        ARENA_FREE_NOSIZE(l_Component);
        return true;
    }
    return false;
}

void VulkanDevice::configureOneTimeQueue(const QueueSelection p_Queue)
{
    m_OneTimeQueue = p_Queue;
}

void VulkanDevice::initializeOneTimeCommandPool(const uint32_t p_ThreadID)
{
    if (!m_ThreadCommandInfos.contains(p_ThreadID))
    {
        m_ThreadCommandInfos[p_ThreadID] = {};
    }
    ThreadCommandInfo& l_ThreadInfo = m_ThreadCommandInfos[p_ThreadID];

    if (l_ThreadInfo.oneTimePool != VK_NULL_HANDLE)
    {
        return;
    }

    VkCommandPoolCreateInfo l_PoolInfo{};
    l_PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    l_PoolInfo.queueFamilyIndex = m_OneTimeQueue.familyIndex;
    l_PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VULKAN_TRY(getTable().vkCreateCommandPool(m_VkHandle, &l_PoolInfo, nullptr, &l_ThreadInfo.oneTimePool));
}

void VulkanDevice::initializeCommandPool(const QueueFamily& p_Family, const ThreadID p_ThreadID, const bool p_AllowBufferReset)
{
    if (!m_ThreadCommandInfos.contains(p_ThreadID))
    {
        m_ThreadCommandInfos[p_ThreadID] = {};
    }

    ThreadCommandInfo& l_ThreadInfo = m_ThreadCommandInfos[p_ThreadID];
    if (!l_ThreadInfo.commandPools.contains(p_Family.index))
    {
        VkCommandPoolCreateInfo l_PoolInfo{};
        l_PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        l_PoolInfo.queueFamilyIndex = p_Family.index;
        l_PoolInfo.flags = p_AllowBufferReset ? VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : 0;

        VULKAN_TRY(getTable().vkCreateCommandPool(m_VkHandle, &l_PoolInfo, nullptr, &l_ThreadInfo.commandPools[p_Family.index]));
        LOG_DEBUG("Created main command pool for thread ", p_ThreadID, " and family ", p_Family.index);
    }
}

ResourceID VulkanDevice::createCommandBuffer(const QueueFamily& p_Family, const ThreadID p_ThreadID, const bool p_IsSecondary)
{
    initializeCommandPool(p_Family, p_ThreadID);

    VulkanCommandBuffer::TypeFlags l_Type = 0;
    VkCommandBufferAllocateInfo l_AllocInfo{};
    l_AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    l_AllocInfo.commandPool = m_ThreadCommandInfos[p_ThreadID].commandPools[p_Family.index];
    if (p_IsSecondary)
    {
        l_AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        l_Type = VulkanCommandBuffer::TypeFlagBits::SECONDARY;
    }
    else
    {
        l_AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    }
    l_AllocInfo.commandBufferCount = 1;

    VkCommandBuffer l_CommandBuffer;
    VULKAN_TRY(getTable().vkAllocateCommandBuffers(m_VkHandle, &l_AllocInfo, &l_CommandBuffer));
    LOG_DEBUG("Allocated command buffer for thread ", p_ThreadID, " and family ", p_Family.index);

    if (!m_CommandBuffers.contains(p_ThreadID))
    {
        m_CommandBuffers[p_ThreadID] = {};
    }

    m_CommandBuffers[p_ThreadID].buffers.push_back({m_ID, l_CommandBuffer, l_Type, p_Family.index, p_ThreadID});
    return m_CommandBuffers[p_ThreadID].buffers.back().getID();
}

ResourceID VulkanDevice::createOneTimeCommandBuffer(ThreadID p_ThreadID)
{
    initializeOneTimeCommandPool(p_ThreadID);

    VkCommandBufferAllocateInfo l_AllocInfo{};
    l_AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    l_AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    l_AllocInfo.commandPool = m_ThreadCommandInfos[p_ThreadID].oneTimePool;
    l_AllocInfo.commandBufferCount = 1;

    VkCommandBuffer l_CommandBuffer;
    VULKAN_TRY(getTable().vkAllocateCommandBuffers(m_VkHandle, &l_AllocInfo, &l_CommandBuffer));
    LOG_DEBUG("Allocated one time command buffer for thread ", p_ThreadID);

    if (!m_CommandBuffers.contains(p_ThreadID))
    {
        m_CommandBuffers[p_ThreadID] = {};
    }

    m_CommandBuffers[p_ThreadID].buffers.push_back({m_ID, l_CommandBuffer, VulkanCommandBuffer::TypeFlagBits::ONE_TIME, m_OneTimeQueue.familyIndex, p_ThreadID});
    return m_CommandBuffers[p_ThreadID].buffers.back().getID();
}

ResourceID VulkanDevice::getOrCreateCommandBuffer(const QueueFamily& p_Family, const ThreadID p_ThreadID, const VulkanCommandBuffer::TypeFlags p_Flags)
{
    for (const VulkanCommandBuffer& l_Buffer : m_CommandBuffers[p_ThreadID].buffers)
    {
        if (l_Buffer.m_FamilyIndex == p_Family.index && l_Buffer.m_ThreadID == p_ThreadID && l_Buffer.m_Flags == p_Flags)
        {
            LOG_DEBUG("Reusing command buffer for thread ", p_ThreadID, " and family ", p_Family.index);
            return l_Buffer.getID();
        }
    }

    return createCommandBuffer(p_Family, p_ThreadID, (p_Flags & VulkanCommandBuffer::TypeFlagBits::SECONDARY) != 0);
}

VulkanCommandBuffer& VulkanDevice::getCommandBuffer(const ResourceID p_ID, const ThreadID p_ThreadID)
{
    for (VulkanCommandBuffer& l_Buffer : m_CommandBuffers[p_ThreadID].buffers)
    {
        if (l_Buffer.m_ID == p_ID)
        {
            return l_Buffer;
        }
    }

    LOG_DEBUG("Command buffer search failed out of ", m_CommandBuffers.size(), " command buffers");
    throw std::runtime_error("Command buffer (ID:" + std::to_string(p_ID) + ") not found");
}

const VulkanCommandBuffer& VulkanDevice::getCommandBuffer(const ResourceID p_ID, const ThreadID p_ThreadID) const
{
    return const_cast<VulkanDevice*>(this)->getCommandBuffer(p_ID, p_ThreadID);
}

void VulkanDevice::freeCommandBuffer(const VulkanCommandBuffer& p_CommandBuffer, const ThreadID p_ThreadID)
{
    freeCommandBuffer(p_CommandBuffer.m_ID, p_ThreadID);
}

void VulkanDevice::freeCommandBuffer(const ResourceID p_ID, const ThreadID p_ThreadID)
{
    for (auto l_It = m_CommandBuffers[p_ThreadID].buffers.begin(); l_It != m_CommandBuffers[p_ThreadID].buffers.end(); ++l_It)
    {
        if (l_It->m_ID == p_ID)
        {
            l_It->free();
            m_CommandBuffers[p_ThreadID].buffers.erase(l_It);
            break;
        }
    }
}

std::vector<VulkanFramebuffer*> VulkanDevice::getFramebuffers() const
{
    std::vector<VulkanFramebuffer*> l_Framebuffers;
    for (VulkanDeviceSubresource* const l_Component : m_Subresources | std::views::values)
    {
        if (VulkanFramebuffer* l_Framebuffer = dynamic_cast<VulkanFramebuffer*>(l_Component))
        {
            l_Framebuffers.push_back(l_Framebuffer);
        }
    }
    return l_Framebuffers;
}

uint32_t VulkanDevice::getFramebufferCount() const
{
    uint32_t l_Count = 0;
    for (VulkanDeviceSubresource* const l_Component : m_Subresources | std::views::values)
    {
        if (dynamic_cast<VulkanFramebuffer*>(l_Component))
        {
            l_Count++;
        }
    }
    return l_Count;
}

ResourceID VulkanDevice::createFramebuffer(const VkExtent3D p_Size, const ResourceID p_RenderPass, const std::span<const VkImageView> p_Attachments)
{
    VkFramebufferCreateInfo l_FramebufferInfo{};
    l_FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    l_FramebufferInfo.renderPass = *getRenderPass(p_RenderPass);
    l_FramebufferInfo.attachmentCount = static_cast<uint32_t>(p_Attachments.size());
    l_FramebufferInfo.pAttachments = p_Attachments.data();
    l_FramebufferInfo.width = p_Size.width;
    l_FramebufferInfo.height = p_Size.height;
    l_FramebufferInfo.layers = p_Size.depth;

    VkFramebuffer l_Framebuffer;
    VULKAN_TRY(getTable().vkCreateFramebuffer(m_VkHandle, &l_FramebufferInfo, nullptr, &l_Framebuffer));

    VulkanFramebuffer* l_NewRes = ARENA_ALLOC(VulkanFramebuffer){m_ID, l_Framebuffer};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created framebuffer (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createAndAllocateBuffer(const VulkanMemoryAllocator::MemoryPreferences& p_MemoryPreferences, const VulkanBuffer::Config& p_Config)
{
    VkBufferCreateInfo l_BufferInfo{};
    l_BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    l_BufferInfo.size = p_Config.size;
    l_BufferInfo.usage = p_Config.usage;
    l_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    l_BufferInfo.flags = 0;
    if (p_Config.ownerQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        l_BufferInfo.queueFamilyIndexCount = 1;
        l_BufferInfo.pQueueFamilyIndices = &p_Config.ownerQueueFamilyIndex;
    }

    const VulkanMemoryAllocator::AllocationReturn l_Ret = m_MemoryAllocator.createBuffer(l_BufferInfo, p_MemoryPreferences);

    VulkanBuffer* l_NewRes = ARENA_ALLOC(VulkanBuffer){m_ID, l_Ret.as<VkBuffer>(), p_Config.size};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    l_NewRes->setBoundMemory(l_Ret.allocation);
    LOG_DEBUG("Created and allocated buffer (ID:", l_NewRes->getID(), ") with size ", VulkanMemoryAllocator::compactBytes(l_NewRes->getSize()));
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createBuffer(const VulkanBuffer::Config& p_Config)
{
    VkBufferCreateInfo l_BufferInfo{};
    l_BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    l_BufferInfo.size = p_Config.size;
    l_BufferInfo.usage = p_Config.usage;
    l_BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    l_BufferInfo.flags = 0;
    if (p_Config.ownerQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        l_BufferInfo.queueFamilyIndexCount = 1;
        l_BufferInfo.pQueueFamilyIndices = &p_Config.ownerQueueFamilyIndex;
    }

    VkBuffer l_Buffer;
    VULKAN_TRY(getTable().vkCreateBuffer(m_VkHandle, &l_BufferInfo, nullptr, &l_Buffer));

    VulkanBuffer* l_NewRes = ARENA_ALLOC(VulkanBuffer){m_ID, l_Buffer, p_Config.size};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created buffer (ID:", l_NewRes->getID(), ") with size ", VulkanMemoryAllocator::compactBytes(l_NewRes->getSize()));
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createAndAllocateImage(const VulkanMemoryAllocator::MemoryPreferences& p_MemoryPreferences, const VulkanImage::Config& p_Config)
{
    VkImageCreateInfo l_ImageInfo{};
    l_ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    l_ImageInfo.imageType = p_Config.type;
    l_ImageInfo.format = p_Config.format;
    l_ImageInfo.extent = p_Config.extent;
    l_ImageInfo.mipLevels = 1;
    l_ImageInfo.arrayLayers = 1;
    l_ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    l_ImageInfo.tiling = p_Config.tiling;
    l_ImageInfo.usage = p_Config.usage;
    l_ImageInfo.flags = p_Config.flags;
    l_ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    l_ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    l_ImageInfo.queueFamilyIndexCount = 0;
    l_ImageInfo.pQueueFamilyIndices = nullptr;

    const VulkanMemoryAllocator::AllocationReturn l_Ret = m_MemoryAllocator.createImage(l_ImageInfo, p_MemoryPreferences);

    VulkanImage* l_NewRes = ARENA_ALLOC(VulkanImage) { m_ID, l_Ret.as<VkImage>(), p_Config.extent, p_Config.type, VK_IMAGE_LAYOUT_UNDEFINED };
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    l_NewRes->setBoundMemory(l_Ret.allocation);
    LOG_DEBUG("Created and allocated image (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createImage(const VulkanImage::Config& p_Config)
{
    VkImageCreateInfo l_ImageInfo{};
    l_ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    l_ImageInfo.imageType = p_Config.type;
    l_ImageInfo.format = p_Config.format;
    l_ImageInfo.extent = p_Config.extent;
    l_ImageInfo.mipLevels = 1;
    l_ImageInfo.arrayLayers = 1;
    l_ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    l_ImageInfo.tiling = p_Config.tiling;
    l_ImageInfo.usage = p_Config.usage;
    l_ImageInfo.flags = p_Config.flags;
    l_ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    l_ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    l_ImageInfo.queueFamilyIndexCount = 0;
    l_ImageInfo.pQueueFamilyIndices = nullptr;

    VkImage l_Image;
    VULKAN_TRY(getTable().vkCreateImage(m_VkHandle, &l_ImageInfo, nullptr, &l_Image));

    VulkanImage* l_NewRes = ARENA_ALLOC(VulkanImage){m_ID, l_Image, p_Config.extent, p_Config.type, VK_IMAGE_LAYOUT_UNDEFINED};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created image (ID:", l_NewRes->getID(), ")");

    return l_NewRes->getID();
}

void VulkanDevice::configureStagingBuffer(const VkDeviceSize p_Size, const QueueSelection& p_Queue, const bool p_ForceAllowStagingMemory)
{
    if (m_StagingBufferInfo.stagingBuffer != UINT32_MAX)
    {
        freeStagingBuffer();
    }
    constexpr VulkanMemoryAllocator::MemoryPreferences PREFS{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .preferredProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    m_StagingBufferInfo.stagingBuffer = createAndAllocateBuffer(PREFS, {p_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, p_Queue.familyIndex});
    m_StagingBufferInfo.queue = p_Queue;
}

VkDeviceSize VulkanDevice::getStagingBufferSize() const
{
    if (m_StagingBufferInfo.stagingBuffer == UINT32_MAX)
    {
        return 0;
    }
    return getBuffer(m_StagingBufferInfo.stagingBuffer).getSize();
}

bool VulkanDevice::freeStagingBuffer()
{
    if (m_StagingBufferInfo.stagingBuffer != UINT32_MAX)
    {
        {
            VulkanBuffer& l_StagingBuffer = getBuffer(m_StagingBufferInfo.stagingBuffer);
            l_StagingBuffer.free();
        }
        m_StagingBufferInfo.stagingBuffer = UINT32_MAX;
        return true;
    }
    return false;
}

void* VulkanDevice::mapStagingBuffer(const VkDeviceSize p_Size, const VkDeviceSize p_Offset)
{
    return getBuffer(m_StagingBufferInfo.stagingBuffer).map(p_Size, p_Offset);
}

void VulkanDevice::unmapStagingBuffer()
{
    getBuffer(m_StagingBufferInfo.stagingBuffer).unmap();
}

VulkanDevice::StagingBufferInfo VulkanDevice::getStagingBufferData() const
{
    return m_StagingBufferInfo;
}

ResourceID VulkanDevice::createRenderPass(const VulkanRenderPassBuilder& p_Builder, const VkRenderPassCreateFlags p_Flags)
{
    VkRenderPassCreateInfo l_RenderPassInfo{};
    l_RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    l_RenderPassInfo.attachmentCount = static_cast<uint32_t>(p_Builder.m_Attachments.size());
    l_RenderPassInfo.pAttachments = p_Builder.m_Attachments.data();
    l_RenderPassInfo.subpassCount = static_cast<uint32_t>(p_Builder.m_Subpasses.size());
    TRANS_VECTOR(l_Subpasses, VkSubpassDescription);
    for (const VulkanRenderPassBuilder::SubpassInfo& l_Subpass : p_Builder.m_Subpasses)
    {
        VkSubpassDescription l_SubpassInfo{};
        l_SubpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        l_SubpassInfo.flags = l_Subpass.flags;
        l_SubpassInfo.colorAttachmentCount = static_cast<uint32_t>(l_Subpass.colorAttachments.size());
        l_SubpassInfo.pColorAttachments = l_Subpass.colorAttachments.data();
        l_SubpassInfo.pResolveAttachments = l_Subpass.resolveAttachments.data();
        l_SubpassInfo.pDepthStencilAttachment = l_Subpass.hasDepthStencilAttachment ? &l_Subpass.depthStencilAttachment : VK_NULL_HANDLE;
        l_SubpassInfo.inputAttachmentCount = static_cast<uint32_t>(l_Subpass.inputAttachments.size());
        l_SubpassInfo.pInputAttachments = l_Subpass.inputAttachments.data();
        l_SubpassInfo.preserveAttachmentCount = static_cast<uint32_t>(l_Subpass.preserveAttachments.size());
        l_SubpassInfo.pPreserveAttachments = l_Subpass.preserveAttachments.data();

        l_Subpasses.push_back(l_SubpassInfo);
    }
    l_RenderPassInfo.pSubpasses = l_Subpasses.data();
    l_RenderPassInfo.dependencyCount = static_cast<uint32_t>(p_Builder.m_Dependencies.size());
    l_RenderPassInfo.pDependencies = p_Builder.m_Dependencies.data();
    l_RenderPassInfo.flags = p_Flags;

    VkRenderPass l_RenderPass;
    VULKAN_TRY(getTable().vkCreateRenderPass(m_VkHandle, &l_RenderPassInfo, nullptr, &l_RenderPass));

    VulkanRenderPass* l_NewRes = ARENA_ALLOC(VulkanRenderPass){m_ID, l_RenderPass};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created renderpass (ID:", l_NewRes->getID(), ") with ", p_Builder.m_Attachments.size(), " attachment(s) and ", p_Builder.m_Subpasses.size(), " subpass(es)");

    return l_NewRes->getID();
}

ResourceID VulkanDevice::createPipelineLayout(const std::span<const ResourceID> p_DescriptorSetLayouts, const std::span<const VkPushConstantRange> p_PushConstantRanges)
{
    TRANS_VECTOR(l_Layouts, VkDescriptorSetLayout);
    l_Layouts.reserve(p_DescriptorSetLayouts.size());
    for (const ResourceID l_ID : p_DescriptorSetLayouts)
    {
        l_Layouts.push_back(getDescriptorSetLayout(l_ID).m_VkHandle);
    }

    VkPipelineLayoutCreateInfo l_PipelineLayoutInfo{};
    l_PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    l_PipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(l_Layouts.size());
    l_PipelineLayoutInfo.pSetLayouts = l_Layouts.data();
    l_PipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(p_PushConstantRanges.size());
    l_PipelineLayoutInfo.pPushConstantRanges = p_PushConstantRanges.data();

    VkPipelineLayout l_Layout;
    VULKAN_TRY(getTable().vkCreatePipelineLayout(m_VkHandle, &l_PipelineLayoutInfo, nullptr, &l_Layout));

    VulkanPipelineLayout* l_NewRes = ARENA_ALLOC(VulkanPipelineLayout){m_ID, l_Layout};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created pipeline layout (ID:", l_NewRes->getID(), ") with ", l_Layouts.size(), " descriptor set layout(s) and ", p_PushConstantRanges.size(), " push constant range(s)");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createDescriptorPool(const std::span<const VkDescriptorPoolSize> p_PoolSizes, const uint32_t p_MaxSets, const VkDescriptorPoolCreateFlags p_Flags)
{
    VkDescriptorPoolCreateInfo l_PoolInfo{};
    l_PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    l_PoolInfo.flags = p_Flags;
    l_PoolInfo.poolSizeCount = static_cast<uint32_t>(p_PoolSizes.size());
    l_PoolInfo.pPoolSizes = p_PoolSizes.data();
    l_PoolInfo.maxSets = p_MaxSets;

    VkDescriptorPool l_DescriptorPool;
    VULKAN_TRY(getTable().vkCreateDescriptorPool(m_VkHandle, &l_PoolInfo, nullptr, &l_DescriptorPool));

    VulkanDescriptorPool* l_NewRes = ARENA_ALLOC(VulkanDescriptorPool){m_ID, l_DescriptorPool, p_Flags};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created descriptor pool (ID:", l_NewRes->getID(), ") with ", p_PoolSizes.size(), " pool size(s) and max sets ", p_MaxSets);
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createDescriptorSetLayout(const std::span<const VkDescriptorSetLayoutBinding> p_Bindings, const VkDescriptorSetLayoutCreateFlags p_Flags)
{
    VkDescriptorSetLayoutCreateInfo l_LayoutInfo{};
    l_LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    l_LayoutInfo.flags = p_Flags;
    l_LayoutInfo.bindingCount = static_cast<uint32_t>(p_Bindings.size());
    l_LayoutInfo.pBindings = p_Bindings.data();

    VkDescriptorSetLayout l_DescriptorSetLayout;
    VULKAN_TRY(getTable().vkCreateDescriptorSetLayout(m_VkHandle, &l_LayoutInfo, nullptr, &l_DescriptorSetLayout));

    VulkanDescriptorSetLayout* l_NewRes = ARENA_ALLOC(VulkanDescriptorSetLayout){m_ID, l_DescriptorSetLayout};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created descriptor set layout (ID:", l_NewRes->getID(), ") with ", p_Bindings.size(), " binding(s)");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createDescriptorSet(ResourceID p_Pool, ResourceID p_Layout)
{
    const VkDescriptorSetLayout l_DescriptorSetLayout = getDescriptorSetLayout(p_Layout).m_VkHandle;
    const VkDescriptorPool l_DescriptorPool = getDescriptorPool(p_Pool).m_VkHandle;

    VkDescriptorSetAllocateInfo l_AllocInfo{};
    l_AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    l_AllocInfo.descriptorPool = l_DescriptorPool;
    l_AllocInfo.descriptorSetCount = 1;
    l_AllocInfo.pSetLayouts = &l_DescriptorSetLayout;

    VkDescriptorSet l_DescriptorSet;
    VULKAN_TRY(getTable().vkAllocateDescriptorSets(m_VkHandle, &l_AllocInfo, &l_DescriptorSet));

    VulkanDescriptorSet* l_NewRes = ARENA_ALLOC(VulkanDescriptorSet){m_ID, p_Pool, l_DescriptorSet};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created descriptor set (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

void VulkanDevice::createDescriptorSets(ResourceID p_Pool, ResourceID p_Layout, const uint32_t p_Count, ResourceID p_Container[])
{
    const VkDescriptorSetLayout l_DescriptorSetLayout = getDescriptorSetLayout(p_Layout).m_VkHandle;
    const VkDescriptorPool l_DescriptorPool = getDescriptorPool(p_Pool).m_VkHandle;

    VkDescriptorSetAllocateInfo l_AllocInfo{};
    l_AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    l_AllocInfo.descriptorPool = l_DescriptorPool;
    l_AllocInfo.descriptorSetCount = p_Count;
    l_AllocInfo.pSetLayouts = &l_DescriptorSetLayout;

    TRANS_VECTOR(l_DescriptorSets, VkDescriptorSet);
    VULKAN_TRY(getTable().vkAllocateDescriptorSets(m_VkHandle, &l_AllocInfo, l_DescriptorSets.data()));

    for (uint32_t i = 0; i < p_Count; i++)
    {
        VulkanDescriptorSet* l_NewRes = ARENA_ALLOC(VulkanDescriptorSet){m_ID, p_Pool, l_DescriptorSets[i]};
        m_Subresources[l_NewRes->getID()] = l_NewRes;
        p_Container[i] = l_NewRes->getID();
        LOG_DEBUG("Created descriptor set (ID:", l_NewRes->getID(), ") in batch");
    }
}

void VulkanDevice::updateDescriptorSets(const std::span<const VkWriteDescriptorSet> p_DescriptorWrites) const
{
    LOG_DEBUG("Updating ", p_DescriptorWrites.size(), " descriptor sets directly from device (ID: ", m_ID, ")");
    getTable().vkUpdateDescriptorSets(m_VkHandle, static_cast<uint32_t>(p_DescriptorWrites.size()), p_DescriptorWrites.data(), 0, nullptr);
}

ResourceID VulkanDevice::createShaderModule(VulkanShader& p_ShaderCode, const VkShaderStageFlagBits p_Stage)
{
    const std::vector<uint32_t> l_Code = p_ShaderCode.getSpirvForStage(p_Stage);
    if (p_ShaderCode.getStatus().status == VulkanShader::Result::NOT_READY)
    {
        LOG_ERR("Tried to create module for stage ", string_VkShaderStageFlagBits(p_Stage), ", but shader is not compiled");
    }
    if (p_ShaderCode.getStatus().status == VulkanShader::Result::FAILED)
    {
        LOG_ERR("Failed to load shader -> ", p_ShaderCode.getStatus().error);
        throw std::runtime_error("Failed to create shader module");
    }

    VkShaderModuleCreateInfo l_CreateInfo{};
    l_CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    l_CreateInfo.codeSize = 4 * l_Code.size();
    l_CreateInfo.pCode = l_Code.data();

    VkShaderModule l_Shader;
    VULKAN_TRY(getTable().vkCreateShaderModule(m_VkHandle, &l_CreateInfo, nullptr, &l_Shader));

    VulkanShaderModule* l_NewRes = ARENA_ALLOC(VulkanShaderModule){m_ID, l_Shader, p_Stage};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created shader (ID:", l_NewRes->getID(), ") and stage ", string_VkShaderStageFlagBits(p_Stage));
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createShaderModule(const std::span<const uint32_t> p_SpirvCode, const VkShaderStageFlagBits p_Stage)
{
    VkShaderModuleCreateInfo l_CreateInfo{};
    l_CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    l_CreateInfo.codeSize = 4 * p_SpirvCode.size();
    l_CreateInfo.pCode = p_SpirvCode.data();

    VkShaderModule l_Shader;
    VULKAN_TRY(getTable().vkCreateShaderModule(m_VkHandle, &l_CreateInfo, nullptr, &l_Shader));

    VulkanShaderModule* l_NewRes = ARENA_ALLOC(VulkanShaderModule){m_ID, l_Shader, p_Stage};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created shader (ID:", l_NewRes->getID(), ") and stage ", string_VkShaderStageFlagBits(p_Stage));
    return l_NewRes->getID();
}

bool VulkanDevice::freeAllShaderModules()
{
    uint32_t l_Count = 0;
    for (VulkanDeviceSubresource* const l_Component : m_Subresources | std::views::values)
    {
        if (const VulkanShaderModule* l_Shader = dynamic_cast<VulkanShaderModule*>(l_Component))
        {
            freeSubresource(l_Shader->getID());
            l_Count++;
        }
    }
    LOG_DEBUG("Freed all shaders (", l_Count, ")");
    return l_Count > 0;
}

ResourceID VulkanDevice::createSemaphore()
{
    VkSemaphoreCreateInfo l_SemaphoreInfo{};
    l_SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore l_Semaphore;
    VULKAN_TRY(getTable().vkCreateSemaphore(m_VkHandle, &l_SemaphoreInfo, nullptr, &l_Semaphore));

    VulkanSemaphore* l_NewRes = ARENA_ALLOC(VulkanSemaphore){m_ID, l_Semaphore};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created semaphore (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createFence(const bool p_Signaled)
{
    VkFenceCreateInfo l_FenceInfo{};
    l_FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    l_FenceInfo.flags = p_Signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkFence l_Fence;
    VULKAN_TRY(getTable().vkCreateFence(m_VkHandle, &l_FenceInfo, nullptr, &l_Fence));

    VulkanFence* l_NewRes = ARENA_ALLOC(VulkanFence){m_ID, l_Fence, p_Signaled};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created fence (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

void VulkanDevice::waitIdle() const
{
    getTable().vkDeviceWaitIdle(m_VkHandle);
}

bool VulkanDevice::isStagingBufferConfigured() const
{
    return m_StagingBufferInfo.stagingBuffer != UINT32_MAX;
}

ResourceID VulkanDevice::createPipeline(const VulkanPipelineBuilder& p_Builder, const ResourceID p_PipelineLayout, const ResourceID p_RenderPass, const uint32_t p_Subpass)
{
    TRANS_VECTOR(l_ShaderModules, VkPipelineShaderStageCreateInfo);
    l_ShaderModules.resize(p_Builder.getShaderStageCount());
    p_Builder.createShaderStages(l_ShaderModules.data());

    VkGraphicsPipelineCreateInfo l_PipelineInfo{};
    l_PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    l_PipelineInfo.stageCount = static_cast<uint32_t>(l_ShaderModules.size());
    l_PipelineInfo.pStages = l_ShaderModules.data();
    l_PipelineInfo.pVertexInputState = &p_Builder.m_VertexInputState;
    l_PipelineInfo.pInputAssemblyState = &p_Builder.m_InputAssemblyState;
    l_PipelineInfo.pViewportState = &p_Builder.m_ViewportState;
    l_PipelineInfo.pRasterizationState = &p_Builder.m_RasterizationState;
    l_PipelineInfo.pMultisampleState = &p_Builder.m_MultisampleState;
    l_PipelineInfo.pDepthStencilState = &p_Builder.m_DepthStencilState;
    l_PipelineInfo.pColorBlendState = &p_Builder.m_ColorBlendState;
    l_PipelineInfo.pDynamicState = &p_Builder.m_DynamicState;
    if (p_Builder.m_TesellationStateEnabled)
    {
        l_PipelineInfo.pTessellationState = &p_Builder.m_TessellationState;
    }

    l_PipelineInfo.layout = getPipelineLayout(p_PipelineLayout).m_VkHandle;
    l_PipelineInfo.renderPass = getRenderPass(p_RenderPass).m_VkHandle;
    l_PipelineInfo.subpass = p_Subpass;
    l_PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline l_Pipeline;
    VULKAN_TRY(getTable().vkCreateGraphicsPipelines(m_VkHandle, VK_NULL_HANDLE, 1, &l_PipelineInfo, nullptr, &l_Pipeline));

    VulkanPipeline* l_NewRes = ARENA_ALLOC(VulkanPipeline){m_ID, l_Pipeline, p_PipelineLayout, p_RenderPass, p_Subpass};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created pipeline (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

ResourceID VulkanDevice::createComputePipeline(const ResourceID p_Layout, const ResourceID p_Shader, const std::string_view p_Entrypoint)
{
    VkPipelineShaderStageCreateInfo l_StageInfo{};
    const VulkanShaderModule& l_Shader = getShaderModule(p_Shader);
    l_StageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    l_StageInfo.stage = l_Shader.m_Stage;
    l_StageInfo.module = l_Shader.m_VkHandle;
    l_StageInfo.pName = p_Entrypoint.data();

    VkComputePipelineCreateInfo l_PipelineInfo{};
    l_PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    l_PipelineInfo.stage = l_StageInfo;
    l_PipelineInfo.layout = getPipelineLayout(p_Layout).m_VkHandle;

    VkPipeline l_Pipeline;
    VULKAN_TRY(getTable().vkCreateComputePipelines(m_VkHandle, VK_NULL_HANDLE, 1, &l_PipelineInfo, nullptr, &l_Pipeline));
    VulkanPipeline* l_NewRes = ARENA_ALLOC(VulkanPipeline){m_ID, l_Pipeline, p_Layout, UINT32_MAX, UINT32_MAX};
    m_Subresources[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created compute pipeline (ID:", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

bool VulkanDevice::isExtensionEnabled(const std::string_view p_Extension) const
{
    if (m_ExtensionManager == nullptr)
    {
        return false;
    }
    return m_ExtensionManager->containsExtension(p_Extension);
}

bool VulkanDevice::free()
{
    for (const ThreadCmdBuffers& l_CommandBuffers : m_CommandBuffers | std::views::values)
    {
        for (const VulkanCommandBuffer& l_Buffer : l_CommandBuffers.buffers)
        {
            getTable().vkFreeCommandBuffers(m_VkHandle, m_ThreadCommandInfos[l_Buffer.m_ThreadID].commandPools[l_Buffer.m_FamilyIndex], 1, &l_Buffer.m_VkHandle);
        }
    }

    for (const ThreadCommandInfo& l_ThreadInfo : m_ThreadCommandInfos | std::views::values)
    {
        for (const VkCommandPool l_CommandPool : l_ThreadInfo.commandPools | std::views::values)
        {
            if (l_CommandPool != VK_NULL_HANDLE)
            {
                getTable().vkDestroyCommandPool(m_VkHandle, l_CommandPool, nullptr);
            }
        }
        if (l_ThreadInfo.oneTimePool != VK_NULL_HANDLE)
        {
            getTable().vkDestroyCommandPool(m_VkHandle, l_ThreadInfo.oneTimePool, nullptr);
        }
    }

    m_ThreadCommandInfos.clear();

    TRANS_VECTOR(l_Subresources, ResourceID);
    l_Subresources.reserve(m_Subresources.size());
    for (const VulkanDeviceSubresource* l_Component : m_Subresources | std::views::values)
    {
        l_Subresources.push_back(l_Component->getID());
    }
    for (const ResourceID l_ID : l_Subresources)
    {
        freeSubresource(l_ID);
    }

    if (m_ExtensionManager != nullptr)
    {
        m_ExtensionManager->freeExtensions();
    }

    m_MemoryAllocator.free();

    if (!m_VkHandle)
    {
        return false;
    }
    getTable().vkDestroyDevice(m_VkHandle, nullptr);
    LOG_DEBUG("Freed device (ID: ", m_ID, ")");
    m_VkHandle = VK_NULL_HANDLE;
    return true;
}

VkCommandPool VulkanDevice::getCommandPool(const uint32_t p_QueueFamilyIndex, ThreadID p_ThreadID, const VulkanCommandBuffer::TypeFlags p_Flags)
{
    if ((p_Flags & VulkanCommandBuffer::TypeFlagBits::ONE_TIME) != 0)
    {
        return m_ThreadCommandInfos[p_ThreadID].oneTimePool;
    }
    return m_ThreadCommandInfos[p_ThreadID].commandPools[p_QueueFamilyIndex];
}

VulkanDevice::VulkanDevice(const VulkanGPU p_PhysicalDevice, const VkDevice p_Device, VulkanDeviceExtensionManager* p_ExtensionManager)
    : m_VkHandle(p_Device), m_PhysicalDevice(p_PhysicalDevice), m_ExtensionManager(p_ExtensionManager)
{
    m_ExtensionManager->setDevice(getID());
    volkLoadDeviceTable(&m_VolkDeviceTable, m_VkHandle);

    m_MemoryAllocator = VulkanMemoryAllocator{*this};
}

void VulkanDevice::insertImage(VulkanImage* p_Image)
{
    if (m_Subresources.contains(p_Image->getID()))
    {
        LOG_DEBUG("Image with ID ", p_Image->getID(), " already exists, not inserting again");
        return;
    }
    m_Subresources[p_Image->getID()] = p_Image;
    LOG_DEBUG("Inserted image (ID:", p_Image->getID(), ") into device");
}
