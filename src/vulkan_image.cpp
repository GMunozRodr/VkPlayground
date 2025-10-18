#include "vulkan_image.hpp"

#include <ranges>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#include "utils/logger.hpp"
#include "vulkan_base.hpp"
#include "vulkan_command_buffer.hpp"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"

VulkanImageView::VulkanImageView(const ResourceID p_Device, const VkImageView p_VkHandle)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_VkHandle) {}

VulkanImageSampler::VulkanImageSampler(const ResourceID p_Device, const VkSampler p_VkHandle)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_VkHandle) {}

void VulkanImageSampler::free()
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    l_Device.getTable().vkDestroySampler(*l_Device, m_VkHandle, nullptr);
    Logger::print(Logger::DEBUG, "Destroyed image sampler ", m_ID);
}

void VulkanImageView::free()
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    l_Device.getTable().vkDestroyImageView(*l_Device, m_VkHandle, nullptr);
    Logger::print(Logger::DEBUG, "Destroyed image view ", m_ID);
}

VkMemoryRequirements VulkanImage::getMemoryRequirements() const
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkMemoryRequirements l_Requirements;
    l_Device.getTable().vkGetImageMemoryRequirements(l_Device.m_VkHandle, m_VkHandle, &l_Requirements);
    return l_Requirements;
}

void VulkanImage::allocate(const MemoryPreferences p_Preferences)
{
    Logger::pushContext("Image memory");
    VulkanMemArray::allocate(p_Preferences);
    Logger::popContext();
}

VkExtent3D VulkanImage::getSize() const
{
    return m_Size;
}

uint32_t VulkanImage::getFlatSize() const
{
    return m_Size.width * m_Size.height * m_Size.depth;
}

VkImageType VulkanImage::getType() const
{
    return m_Type;
}

VkImageLayout VulkanImage::getLayout() const
{
    return m_Layout;
}

uint32_t VulkanImage::getQueue() const
{
    return m_QueueFamilyIndex;
}

void VulkanImage::setLayout(const VkImageLayout p_Layout)
{
    m_Layout = p_Layout;
}

void VulkanImage::setQueue(const uint32_t p_QueueFamilyIndex)
{
    m_QueueFamilyIndex = p_QueueFamilyIndex;
}

VkImage VulkanImage::operator*() const
{
    return m_VkHandle;
}

ResourceID VulkanImage::createImageView(const VkFormat p_Format, const VkImageAspectFlags p_AspectFlags)
{
    VkImageViewType l_Type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    switch (m_Type)
    {
    case VK_IMAGE_TYPE_1D:
        l_Type = VK_IMAGE_VIEW_TYPE_1D;
        break;
    case VK_IMAGE_TYPE_2D:
        l_Type = VK_IMAGE_VIEW_TYPE_2D;
        break;
    case VK_IMAGE_TYPE_3D:
        l_Type = VK_IMAGE_VIEW_TYPE_3D;
        break;
    default:
        throw std::invalid_argument("Invalid image type on create view, requested: " + std::to_string(l_Type));
    }

    VkImageViewCreateInfo l_CreateInfo = {};
    l_CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    l_CreateInfo.image = m_VkHandle;
    l_CreateInfo.viewType = l_Type;
    l_CreateInfo.format = p_Format;
    l_CreateInfo.subresourceRange.aspectMask = p_AspectFlags;
    l_CreateInfo.subresourceRange.baseMipLevel = 0;
    l_CreateInfo.subresourceRange.levelCount = 1;
    l_CreateInfo.subresourceRange.baseArrayLayer = 0;
    l_CreateInfo.subresourceRange.layerCount = 1;

    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkImageView l_ImageView;
    VULKAN_TRY(l_Device.getTable().vkCreateImageView(l_Device.m_VkHandle, &l_CreateInfo, nullptr, &l_ImageView));

    VulkanImageView* l_ImageViewObj = ARENA_ALLOC(VulkanImageView)(getDeviceID(), l_ImageView);
    m_ImageViews.emplace(l_ImageViewObj->getID(), l_ImageViewObj);
    Logger::print(Logger::DEBUG, "Created image view ", l_ImageViewObj->getID(), " for image ", m_ID);
    return l_ImageViewObj->getID();
}

VulkanImageView& VulkanImage::getImageView(const ResourceID p_ImageView)
{
    if (m_ImageViews.contains(p_ImageView))
    {
        return *m_ImageViews.at(p_ImageView);
    }
    throw std::runtime_error("Tried to get image view that doesn't belong to image " + std::to_string(m_ID));
}

const VulkanImageView& VulkanImage::getImageView(const ResourceID p_ImageView) const
{
    if (m_ImageViews.contains(p_ImageView))
    {
        return *m_ImageViews.at(p_ImageView);
    }
    throw std::runtime_error("Tried to get image view that doesn't belong to image " + std::to_string(m_ID));
}

VulkanImageSampler& VulkanImage::getSampler(const ResourceID p_Sampler)
{
    if (m_Samplers.contains(p_Sampler))
    {
        return *m_Samplers.at(p_Sampler);
    }
    throw std::runtime_error("Tried to get sampler that doesn't belong to image " + std::to_string(m_ID));
}

const VulkanImageSampler& VulkanImage::getSampler(const ResourceID p_Sampler) const
{
    if (m_Samplers.contains(p_Sampler))
    {
        return *m_Samplers.at(p_Sampler);
    }
    throw std::runtime_error("Tried to get sampler that doesn't belong to image " + std::to_string(m_ID));
}

VulkanImageView* VulkanImage::getImageViewPtr(const ResourceID p_ImageView) const
{
    if (m_ImageViews.contains(p_ImageView))
    {
        return m_ImageViews.at(p_ImageView);
    }
    return nullptr;
}

void VulkanImage::freeImageView(const ResourceID p_ImageView)
{
    VulkanImageView* l_ImageView = getImageViewPtr(p_ImageView);
    if (l_ImageView == nullptr)
    {
        throw std::runtime_error("Tried to free image view that doesn't belong to image " + std::to_string(m_ID));
    }
    l_ImageView->free();
    ARENA_FREE(l_ImageView, sizeof(VulkanImageView));
    m_ImageViews.erase(p_ImageView);
}

void VulkanImage::freeImageView(const VulkanImageView& p_ImageView)
{
    freeImageView(p_ImageView.getID());
}

ResourceID VulkanImage::createSampler(const VkFilter p_Filter, const VkSamplerAddressMode p_SamplerAddressMode)
{
    VkSamplerCreateInfo l_CreateInfo = {};
    l_CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    l_CreateInfo.magFilter = p_Filter;
    l_CreateInfo.minFilter = p_Filter;
    l_CreateInfo.addressModeU = p_SamplerAddressMode;
    l_CreateInfo.addressModeV = p_SamplerAddressMode;
    l_CreateInfo.addressModeW = p_SamplerAddressMode;
    l_CreateInfo.anisotropyEnable = VK_FALSE;
    l_CreateInfo.maxAnisotropy = 0.0f;
    l_CreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    l_CreateInfo.unnormalizedCoordinates = VK_FALSE;
    l_CreateInfo.compareEnable = VK_FALSE;
    l_CreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    l_CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    l_CreateInfo.mipLodBias = 0.0f;
    l_CreateInfo.minLod = 0.0f;
    l_CreateInfo.maxLod = 0.0f;

    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkSampler l_Sampler;
    VULKAN_TRY(l_Device.getTable().vkCreateSampler(l_Device.m_VkHandle, &l_CreateInfo, nullptr, &l_Sampler));

    VulkanImageSampler* l_SamplerObj = ARENA_ALLOC(VulkanImageSampler)(getDeviceID(), l_Sampler);
    m_Samplers.emplace(l_SamplerObj->getID(), l_SamplerObj);
    return l_SamplerObj->getID();
}

VulkanImageSampler* VulkanImage::getSamplerPtr(const ResourceID p_Sampler) const
{
    if (m_Samplers.contains(p_Sampler))
    {
        return m_Samplers.at(p_Sampler);
    }
    return nullptr;
}

void VulkanImage::freeSampler(const ResourceID p_Sampler)
{
    VulkanImageSampler* l_Sampler = getSamplerPtr(p_Sampler);
    if (l_Sampler == nullptr)
    {
        throw std::runtime_error("Tried to free sampler that doesn't belong to image " + std::to_string(m_ID));
    }
    l_Sampler->free();
    ARENA_FREE(l_Sampler, sizeof(VulkanImageSampler));
    m_Samplers.erase(p_Sampler);
}

void VulkanImage::freeSampler(const VulkanImageSampler& p_Sampler)
{
    freeSampler(p_Sampler.getID());
}

VulkanImage::VulkanImage(const ResourceID p_Device, const VkImage p_VkHandle, const VkExtent3D p_Size, const VkImageType p_Type, const VkImageLayout p_Layout)
    : VulkanMemArray(p_Device), m_Size(p_Size), m_Type(p_Type), m_Layout(p_Layout), m_VkHandle(p_VkHandle) {}

void VulkanImage::setBoundMemory(const VmaAllocation p_Allocation)
{
    m_Allocation = p_Allocation;
}

void VulkanImage::free()
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    for (VulkanImageView* l_ImageView : m_ImageViews | std::views::values)
    {
        l_ImageView->free();
        ARENA_FREE(l_ImageView, sizeof(VulkanImageView));
    }
    m_ImageViews.clear();

    for (VulkanImageSampler* l_Sampler : m_Samplers | std::views::values)
    {
        l_Sampler->free();
        ARENA_FREE(l_Sampler, sizeof(VulkanImageSampler));
    }
    m_Samplers.clear();

    LOG_DEBUG("Freed image (ID: ", m_ID, ") with ", m_ImageViews.size(), " image views and ", m_Samplers.size(), " samplers");
    Logger::pushContext("Image memory free");
    m_ImageViews.clear();
    m_Samplers.clear();

    l_Device.getTable().vkDestroyImage(l_Device.m_VkHandle, m_VkHandle, nullptr);
    m_VkHandle = VK_NULL_HANDLE;

    if (m_Allocation)
    {
        l_Device.m_MemoryAllocator.deallocate(m_Allocation);
        m_Allocation = VK_NULL_HANDLE;
    }

    Logger::popContext();
}
