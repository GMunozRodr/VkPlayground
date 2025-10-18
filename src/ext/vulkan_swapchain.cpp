#include "ext/vulkan_swapchain.hpp"

#include <stdexcept>
#include <ranges>
#include <vulkan/vk_enum_string_helper.h>

#include "vulkan_device.hpp"
#include "vulkan_sync.hpp"
#include "utils/logger.hpp"
#include "../vulkan_base.hpp"

VulkanSwapchainExtension* VulkanSwapchainExtension::get(const VulkanDevice& p_Device)
{
    return p_Device.getExtensionManager()->getExtension<VulkanSwapchainExtension>(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

VulkanSwapchainExtension* VulkanSwapchainExtension::get(const ResourceID p_DeviceID)
{
    return VulkanContext::getDevice(p_DeviceID).getExtensionManager()->getExtension<VulkanSwapchainExtension>(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

ResourceID VulkanSwapchainExtension::createSwapchain(const VkSurfaceKHR p_Surface, const VkExtent2D p_Extent, const VkSurfaceFormatKHR p_DesiredFormat, VkPresentModeKHR p_PresentMode, const uint32_t p_OldSwapchain)
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanGPU l_PhysicalDevice = l_Device.getGPU();
    const VkSurfaceFormatKHR l_SelectedFormat = l_PhysicalDevice.getClosestFormat(p_Surface, p_DesiredFormat);

    const VkSurfaceCapabilitiesKHR l_Capabilities = l_PhysicalDevice.getCapabilities(p_Surface);

    VkSwapchainCreateInfoKHR l_CreateInfo{};
    l_CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    l_CreateInfo.surface = p_Surface;
    l_CreateInfo.minImageCount = l_Capabilities.minImageCount + 1;
    if (l_Capabilities.maxImageCount > 0 && l_CreateInfo.minImageCount > l_Capabilities.maxImageCount)
    {
        l_CreateInfo.minImageCount = l_Capabilities.maxImageCount;
    }
    l_CreateInfo.imageFormat = l_SelectedFormat.format;
    l_CreateInfo.imageColorSpace = l_SelectedFormat.colorSpace;
    l_CreateInfo.imageExtent = p_Extent;
    l_CreateInfo.imageArrayLayers = 1;
    l_CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    l_CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    l_CreateInfo.preTransform = l_Capabilities.currentTransform;
    l_CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    l_CreateInfo.presentMode = p_PresentMode;
    l_CreateInfo.clipped = VK_TRUE;
    if (p_OldSwapchain != UINT32_MAX)
    {
        l_CreateInfo.oldSwapchain = *getSwapchain(p_OldSwapchain);
    }

    VkSwapchainKHR l_SwapchainHandle;
    VULKAN_TRY(l_Device.getTable().vkCreateSwapchainKHR(*l_Device, &l_CreateInfo, nullptr, &l_SwapchainHandle));

    if (p_OldSwapchain != UINT32_MAX)
    {
        freeSwapchain(p_OldSwapchain);
    }

    VulkanSwapchain* l_NewRes = ARENA_ALLOC(VulkanSwapchain){getDeviceID(), l_SwapchainHandle, p_Extent, l_SelectedFormat, l_CreateInfo.minImageCount};
    m_Swapchains[l_NewRes->getID()] = l_NewRes;
    LOG_DEBUG("Created swapchain (ID: ", l_NewRes->getID(), ")");
    return l_NewRes->getID();
}

VulkanSwapchain& VulkanSwapchainExtension::getSwapchain(const ResourceID p_ID)
{
    return *m_Swapchains.at(p_ID);
}

const VulkanSwapchain& VulkanSwapchainExtension::getSwapchain(const ResourceID p_ID) const
{
    return *m_Swapchains.at(p_ID);
}

bool VulkanSwapchainExtension::freeSwapchain(const ResourceID p_ID)
{
    VulkanSwapchain* l_Swapchain = m_Swapchains.at(p_ID);
    if (l_Swapchain)
    {
        l_Swapchain->free();
        m_Swapchains.erase(p_ID);
        ARENA_FREE(l_Swapchain, sizeof(VulkanSwapchain));
        return true;
    }
    return false;
}

bool VulkanSwapchainExtension::freeSwapchain(const VulkanSwapchain& p_Swapchain)
{
    return freeSwapchain(p_Swapchain.getID());
}

void VulkanSwapchainExtension::free()
{
    for (VulkanSwapchain* l_Swapchain : m_Swapchains | std::views::values)
    {
        l_Swapchain->free();
        ARENA_FREE(l_Swapchain, sizeof(VulkanSwapchain));
    }
    m_Swapchains.clear();
}

VkSwapchainKHR VulkanSwapchain::operator*() const
{
    return m_VkHandle;
}

uint32_t VulkanSwapchain::getMinImageCount() const
{
    return m_MinImageCount;
}

uint32_t VulkanSwapchain::getImageCount() const
{
    return static_cast<uint32_t>(m_Images.size());
}

VkExtent2D VulkanSwapchain::getExtent() const
{
    return m_Extent;
}

VkSurfaceFormatKHR VulkanSwapchain::getFormat() const
{
    return m_Format;
}

uint32_t VulkanSwapchain::acquireNextImage(const uint32_t p_Fence)
{
    if (m_VkHandle == nullptr)
    {
        throw std::runtime_error("Swapchain not created");
    }

    uint32_t l_ImageIndex;
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanSemaphore& l_Semaphore = l_Device.getSemaphore(m_ImageAvailableSemaphore);
    const VkResult l_Result = l_Device.getTable().vkAcquireNextImageKHR(*l_Device, m_VkHandle, UINT64_MAX, *l_Semaphore, p_Fence != UINT32_MAX ? *l_Device.getFence(p_Fence) : nullptr, &l_ImageIndex);
    if (l_Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return UINT32_MAX;
    }
    if (l_Result != VK_SUCCESS && l_Result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    m_NextImage = l_ImageIndex;
    m_WasAcquired = true;
    return l_ImageIndex;
}

VulkanImage& VulkanSwapchain::getImage(const uint32_t p_Index)
{
    return m_Images[p_Index];
}

ResourceID VulkanSwapchain::getImageView(const uint32_t p_Index) const
{
    return m_ImageViews[p_Index];
}

uint32_t VulkanSwapchain::getNextImage() const
{
    return m_NextImage;
}

uint32_t VulkanSwapchain::getImgSemaphore() const
{
    return m_ImageAvailableSemaphore;
}

bool VulkanSwapchain::present(const QueueSelection p_Queue, const std::span<const ResourceID> p_Semaphores)
{
    if (!m_WasAcquired)
    {
        throw std::runtime_error("Tried to present swpachain, but image was not acquired");
    }
    m_WasAcquired = false;

    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    TRANS_VECTOR(l_SemaphoreHandles, VkSemaphore);
    l_SemaphoreHandles.resize(p_Semaphores.size());
    for (uint32_t i = 0; i < p_Semaphores.size(); i++)
    {
        l_SemaphoreHandles[i] = *l_Device.getSemaphore(p_Semaphores[i]);
    }

    VkPresentInfoKHR l_PresentInfo{};
    l_PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    l_PresentInfo.waitSemaphoreCount = static_cast<uint32_t>(l_SemaphoreHandles.size());
    l_PresentInfo.pWaitSemaphores = l_SemaphoreHandles.data();
    l_PresentInfo.swapchainCount = 1;
    l_PresentInfo.pSwapchains = &m_VkHandle;
    l_PresentInfo.pImageIndices = &m_NextImage;

    const VulkanQueue l_Queue = l_Device.getQueue(p_Queue);
    const VkResult l_Result = l_Device.getTable().vkQueuePresentKHR(*l_Queue, &l_PresentInfo);
    if (l_Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false;
    }
    if (l_Result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
    return true;
}

void VulkanSwapchain::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        for (uint32_t l_ImageViewIdx = 0; l_ImageViewIdx < m_ImageViews.size(); l_ImageViewIdx++)
        {
            m_Images[l_ImageViewIdx].freeImageView(m_ImageViews[l_ImageViewIdx]);
        }
        m_ImageViews.clear();
        m_Images.clear();

        VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

        l_Device.freeSemaphore(m_ImageAvailableSemaphore);

        l_Device.getTable().vkDestroySwapchainKHR(l_Device.m_VkHandle, m_VkHandle, nullptr);
        LOG_DEBUG("Freed Swapchain (ID: ", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

VulkanSwapchain::VulkanSwapchain(ResourceID p_Device, const VkSwapchainKHR p_Handle, const VkExtent2D p_Extent, const VkSurfaceFormatKHR p_Format, const uint32_t p_MinImageCount)
    : VulkanDeviceSubresource(p_Device), m_Extent(p_Extent), m_Format(p_Format), m_MinImageCount(p_MinImageCount), m_VkHandle(p_Handle)
{
    VulkanDevice& l_Device = VulkanContext::getDevice(p_Device);

    uint32_t l_ImageCount;
    TRANS_VECTOR(l_Images, VkImage);
    l_Device.getTable().vkGetSwapchainImagesKHR(l_Device.m_VkHandle, m_VkHandle, &l_ImageCount, nullptr);
    l_Images.resize(l_ImageCount);
    l_Device.getTable().vkGetSwapchainImagesKHR(l_Device.m_VkHandle, m_VkHandle, &l_ImageCount, l_Images.data());

    m_Images.reserve(l_ImageCount);
    for (const VkImage l_Image : l_Images)
    {
        m_Images.push_back({p_Device, l_Image, VkExtent3D{p_Extent.width, p_Extent.height, 1}, VK_IMAGE_TYPE_2D, VK_IMAGE_LAYOUT_UNDEFINED});
    }

    m_ImageViews.reserve(m_Images.size());
    for (VulkanImage& l_Image : m_Images)
    {
        m_ImageViews.push_back(l_Image.createImageView(p_Format.format, VK_IMAGE_ASPECT_COLOR_BIT));
        LOG_DEBUG("Created swapchain image view");
    }

    m_ImageAvailableSemaphore = l_Device.createSemaphore();
}
