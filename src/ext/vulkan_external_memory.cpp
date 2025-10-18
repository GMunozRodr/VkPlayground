#include "ext/vulkan_external_memory.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include "vulkan_device.hpp"
#include "utils/logger.hpp"
#include "../vulkan_base.hpp"

VulkanExternalMemoryExtension* VulkanExternalMemoryExtension::get(const VulkanDevice& p_Device)
{
    return p_Device.getExtensionManager()->getExtension<VulkanExternalMemoryExtension>(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
}

VulkanExternalMemoryExtension* VulkanExternalMemoryExtension::get(const ResourceID p_DeviceID)
{
    return VulkanContext::getDevice(p_DeviceID).getExtensionManager()->getExtension<VulkanExternalMemoryExtension>(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
}

VulkanExternalMemoryExtension::VulkanExternalMemoryExtension(const ResourceID p_DeviceID)
    : VulkanDeviceExtension(p_DeviceID)
{
}

ResourceID VulkanExternalMemoryExtension::createExternalImage(const VkImageType p_Type, const VkFormat p_Format, const VkExtent3D p_Extent, const VkImageUsageFlags p_Usage, const VkImageCreateFlags p_Flags, VkImageTiling p_Tiling) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkImageCreateInfo l_ImageInfo{};
	l_ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	l_ImageInfo.imageType = p_Type;
	l_ImageInfo.format = p_Format;
	l_ImageInfo.extent = p_Extent;
	l_ImageInfo.mipLevels = 1;
	l_ImageInfo.arrayLayers = 1;
	l_ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	l_ImageInfo.tiling = p_Tiling;
	l_ImageInfo.usage = p_Usage;
	l_ImageInfo.flags = p_Flags;
	l_ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	l_ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	l_ImageInfo.queueFamilyIndexCount = 0;
	l_ImageInfo.pQueueFamilyIndices = nullptr;

    VkExternalMemoryImageCreateInfo l_ExternInfo{};
    l_ExternInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
#ifdef _WIN32
    l_ExternInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    l_ExternInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
    l_ImageInfo.pNext = &l_ExternInfo;

    VkImage l_Image;
    VULKAN_TRY(l_Device.getTable().vkCreateImage(*l_Device, &l_ImageInfo, nullptr, &l_Image));

    VulkanImage* l_NewRes = ARENA_ALLOC(VulkanImage){getDeviceID(), l_Image, p_Extent, p_Type, VK_IMAGE_LAYOUT_UNDEFINED};
    LOG_DEBUG("Created image (ID:", l_NewRes->getID(), ")");
    l_Device.insertImage(l_NewRes);

	return l_NewRes->getID();
}

void VulkanExternalMemoryExtension::allocateExport(ResourceID p_Resource, VulkanMemoryAllocator::MemoryPreferences p_MemoryProperties) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    VulkanMemArray* l_MemArray = dynamic_cast<VulkanMemArray*>(VulkanContext::getDevice(getDeviceID()).getSubresource(p_Resource));
    if (!l_MemArray)
    {
        LOG_ERR("Cannot allocate external memory for resource ID ", p_Resource, ", unsupported type");
        return;
    }

    VkExportMemoryAllocateInfo l_ExportAlloc = {};
    l_ExportAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#ifdef _WIN32
    l_ExportAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    l_ExportAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    const VkMemoryRequirements l_Requirements = l_MemArray->getMemoryRequirements();
    const uint32_t l_MemType = l_Device.getMemoryAllocator().findMemoryType(l_Requirements, p_MemoryProperties);
    const VulkanMemoryAllocator::PoolPreferences l_PoolPrefs{
        .memoryTypeIndex = l_MemType,
        .pNext = &l_ExportAlloc,
        .pNextIdentifier = std::hash<std::string>{}(string_VkExternalMemoryHandleTypeFlags(l_ExportAlloc.handleTypes))
    };

    const uint32_t l_Pool = l_Device.getMemoryAllocator().getOrCreatePool(l_PoolPrefs);
    p_MemoryProperties.pool = l_Pool;
    p_MemoryProperties.vmaFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    const VmaAllocation l_Alloc = l_Device.getMemoryAllocator().allocateMemArray(p_Resource, p_MemoryProperties);
    l_MemArray->setBoundMemory(l_Alloc);
}

HANDLE VulkanExternalMemoryExtension::getResourceOpaqueHandle(const ResourceID p_Resource) const
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());
    const VulkanMemArray* l_Resource = dynamic_cast<VulkanMemArray*>(l_Device.getSubresource(p_Resource));
    if (!l_Resource)
    {
        LOG_ERR("Cannot get external memory handle for resource ID ", p_Resource, ", unsupported type");
        return nullptr;
    }

#ifdef _WIN32
    VkMemoryGetWin32HandleInfoKHR l_GetWin32 = {};
    l_GetWin32.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    l_GetWin32.memory = l_Device.getMemoryAllocator().getAllocationInfo(l_Resource->getAllocation()).deviceMemory;
    l_GetWin32.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    HANDLE l_ExternalHandle;
    VULKAN_TRY(l_Device.getTable().vkGetMemoryWin32HandleKHR(*l_Device, &l_GetWin32, &l_ExternalHandle));
    
    return l_ExternalHandle;
#else
    VkMemoryGetFdInfoKHR getFd = {};
    getFd.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    l_GetWin32.memory = l_Device.getMemoryAllocator().getAllocationInfo(l_Resource->getAllocation()).deviceMemory;
    getFd.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    HANDLE externalFd = -1;
    VULKAN_TRY(l_Device.getTable().vkGetMemoryFdKHR(*l_Device, &getFd, &externalFd));
    return externalFd;
#endif
}

std::vector<std::string> VulkanExternalMemoryExtension::getExtraExtensionNames()
{
#ifdef _WIN32
    return { "VK_KHR_external_memory_win32" };
#else
    return { VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME };
#endif
}
