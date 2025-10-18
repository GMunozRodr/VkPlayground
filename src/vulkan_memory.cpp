#include "vulkan_memory.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>
#include <Volk/volk.h>

#include "utils/logger.hpp"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"
#include "vulkan_base.hpp"

std::string VulkanMemoryAllocator::compactBytes(const VkDeviceSize p_Bytes)
{
    const char* l_Units[] = {"B", "KB", "MB", "GB", "TB"};
    uint32_t l_Unit = 0;
    float l_Exact = static_cast<float>(p_Bytes);
    while (l_Exact > 1024)
    {
        l_Exact /= 1024;
        l_Unit++;
    }
    return std::to_string(l_Exact) + " " + l_Units[l_Unit];
}

bool VulkanMemoryAllocator::free()
{
    if (m_Allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
        return true;
    }
    return false;
}

const VkPhysicalDeviceMemoryProperties& MemoryStructure::getMemoryProperties() const
{
    return m_MemoryProperties;
}

std::string MemoryStructure::toString() const
{
    const VkPhysicalDeviceMemoryProperties l_MemoryProperties = getMemoryProperties();

    std::string l_Str;
    for (uint32_t l_MemoryHeapIdx = 0; l_MemoryHeapIdx < l_MemoryProperties.memoryHeapCount; l_MemoryHeapIdx++)
    {
        l_Str += "Memory Heap " + std::to_string(l_MemoryHeapIdx) + ":\n";
        l_Str += " - Size: " + VulkanMemoryAllocator::compactBytes(l_MemoryProperties.memoryHeaps[l_MemoryHeapIdx].size) + "\n";
        l_Str += " - Flags: " + string_VkMemoryHeapFlags(l_MemoryProperties.memoryHeaps[l_MemoryHeapIdx].flags) + "\n";
        l_Str += " - Memory Types:\n";
        for (uint32_t l_MemoryTypeIdx = 0; l_MemoryTypeIdx < l_MemoryProperties.memoryTypeCount; l_MemoryTypeIdx++)
        {
            if (l_MemoryProperties.memoryTypes[l_MemoryTypeIdx].heapIndex == l_MemoryHeapIdx)
            {
                l_Str += "    - Memory Type " + std::to_string(l_MemoryTypeIdx) + ": " + string_VkMemoryPropertyFlags(l_MemoryProperties.memoryTypes[l_MemoryTypeIdx].propertyFlags) + "\n";
            }
        }
    }
    return l_Str;
}

std::optional<uint32_t> MemoryStructure::getStagingMemoryType(const uint32_t p_TypeFilter) const
{
    std::vector<uint32_t> l_Types = getMemoryTypes(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, p_TypeFilter);

    if (l_Types.empty())
    {
        return std::nullopt;
    }

    return l_Types.front();
}

std::vector<uint32_t> MemoryStructure::getMemoryTypes(const VkMemoryPropertyFlags p_Properties, const uint32_t p_TypeFilter) const
{
    std::vector<uint32_t> l_SuitableTypes{};
    for (uint32_t l_MemoryTypeIdx = 0; l_MemoryTypeIdx < getMemoryProperties().memoryTypeCount; l_MemoryTypeIdx++)
    {
        if ((p_TypeFilter & (1 << l_MemoryTypeIdx)) && doesMemoryContainProperties(l_MemoryTypeIdx, p_Properties))
        {
            l_SuitableTypes.push_back(l_MemoryTypeIdx);
        }
    }
    return l_SuitableTypes;
}

bool MemoryStructure::doesMemoryContainProperties(const uint32_t p_Type, const VkMemoryPropertyFlags p_Property) const
{
    return (getMemoryProperties().memoryTypes[p_Type].propertyFlags & p_Property) == p_Property;
}

MemoryStructure::MemoryTypeData MemoryStructure::getTypeData(const uint32_t p_MemoryType) const
{
    const VkPhysicalDeviceMemoryProperties l_MemoryProperties = getMemoryProperties();
    return {l_MemoryProperties.memoryTypes[p_MemoryType].propertyFlags, l_MemoryProperties.memoryTypes[p_MemoryType].heapIndex, l_MemoryProperties.memoryHeaps[l_MemoryProperties.memoryTypes[p_MemoryType].heapIndex].size};
}

VkMemoryHeap MemoryStructure::getMemoryTypeHeap(const uint32_t p_MemoryType) const
{
    const VkPhysicalDeviceMemoryProperties l_MemoryProperties = getMemoryProperties();
    return l_MemoryProperties.memoryHeaps[l_MemoryProperties.memoryTypes[p_MemoryType].heapIndex];
}

uint32_t MemoryStructure::getMemoryTypeCount() const
{
    return getMemoryProperties().memoryTypeCount;
}

uint32_t MemoryStructure::getMemoryHeapCount() const
{
    return getMemoryProperties().memoryHeapCount;
}

MemoryStructure::MemoryStructure(const VulkanGPU p_GPU) : m_GPU(p_GPU)
{
    vkGetPhysicalDeviceMemoryProperties(*m_GPU, &m_MemoryProperties);
}

VulkanMemoryAllocator::MemoryPreferences VulkanMemoryAllocator::MemoryPreferences::fromUsage(const VmaMemoryUsage p_Usage, const VmaAllocationCreateFlags p_Flags)
{
    return { .usage = p_Usage, .vmaFlags = p_Flags };
}

bool VulkanMemoryAllocator::PoolPreferences::operator==(const PoolPreferences& p_Other) const
{
    const bool l_Equal = memoryTypeIndex == p_Other.memoryTypeIndex
        && flags == p_Other.flags
        && blockSize == p_Other.blockSize
        && minBlockCount == p_Other.minBlockCount
        && maxBlockCount == p_Other.maxBlockCount
        && priority == p_Other.priority
        && customMinAlignment == p_Other.customMinAlignment;

    if (pNext == nullptr && p_Other.pNext == nullptr)
        return l_Equal;
    if (pNext == nullptr || p_Other.pNext == nullptr)
        return false;
    return l_Equal && pNextIdentifier == p_Other.pNextIdentifier;
}

uint32_t VulkanMemoryAllocator::findMemoryType(const MemoryPreferences& p_Preferences) const
{
    return findMemoryType(p_Preferences, UINT32_MAX);
}

uint32_t VulkanMemoryAllocator::findMemoryType(const VkMemoryRequirements& p_Reqs, const MemoryPreferences& p_Preferences) const
{
    return findMemoryType(p_Preferences, p_Reqs.memoryTypeBits);
}

uint32_t VulkanMemoryAllocator::findMemoryType(const MemoryPreferences& p_Preferences, const uint32_t p_StartingFilter) const
{
    const VkPhysicalDeviceMemoryProperties& l_MemProps = m_MemoryStructure.getMemoryProperties();

    const uint32_t l_Count = l_MemProps.memoryTypeCount;
    const uint32_t l_AllMask = p_StartingFilter & ((1 << l_Count) - 1);

    if (!l_AllMask) 
        return UINT32_MAX;

    VmaAllocationCreateInfo l_Aci = toVmaAllocCI(p_Preferences, 0);
    uint32_t l_Idx = UINT32_MAX;

    l_Aci.memoryTypeBits = l_AllMask;
    if (vmaFindMemoryTypeIndex(m_Allocator, l_AllMask, &l_Aci, &l_Idx) == VK_SUCCESS)
        return l_Idx;

    return UINT32_MAX;
}

VmaAllocation VulkanMemoryAllocator::allocateMemArray(ResourceID p_MemArray, const MemoryPreferences& p_Preferences) const
{
    VulkanDeviceSubresource* l_MemArray = VulkanContext::getDevice(m_Device).getSubresource(p_MemArray);
    if (dynamic_cast<VulkanBuffer*>(l_MemArray))
    {
        return allocateBuffer(p_MemArray, p_Preferences);
    }
    if (dynamic_cast<VulkanImage*>(l_MemArray))
    {
        return allocateImage(p_MemArray, p_Preferences);
    }
    LOG_ERR("Tried to allocate memory for resource", p_MemArray, ": unsupported type");
    return {};
}

VmaAllocation VulkanMemoryAllocator::allocateBuffer(const ResourceID p_Buffer, const MemoryPreferences& p_Preferences) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(m_Device);
    const VulkanBuffer& l_Buffer = l_Device.getBuffer(p_Buffer);

    const VkMemoryRequirements l_Reqs = l_Buffer.getMemoryRequirements();

    const uint32_t l_ForcedTypeIdx = p_Preferences.forceMemoryIndex;

    uint32_t l_Visible = l_Reqs.memoryTypeBits;
    if (l_ForcedTypeIdx != UINT32_MAX) 
    {
        l_Visible &= (1u << l_ForcedTypeIdx);
        if (l_Visible == 0)
        {
            LOG_ERR("Tried to allocate memory for buffer", p_Buffer, ": forced memory type", l_ForcedTypeIdx, "is not compatible with memory type bits", l_Reqs.memoryTypeBits);
            return VK_NULL_HANDLE;
        }
    }

    VmaAllocation l_Alloc{};
    VmaAllocationInfo l_Info{};
    const VmaAllocationCreateInfo l_Aci = toVmaAllocCI(p_Preferences, l_Visible);
    VULKAN_TRY(vmaAllocateMemoryForBuffer(m_Allocator, *l_Buffer, &l_Aci, &l_Alloc, &l_Info));
    VULKAN_TRY(vmaBindBufferMemory(m_Allocator, l_Alloc, *l_Buffer));
    return l_Alloc;
}

VmaAllocation VulkanMemoryAllocator::allocateImage(const ResourceID p_Image, const MemoryPreferences& p_Preferences) const
{
    VulkanDevice& l_Device = VulkanContext::getDevice(m_Device);
    const VulkanImage& l_Image = l_Device.getImage(p_Image);

    const VkMemoryRequirements l_Reqs = l_Image.getMemoryRequirements();

    const uint32_t l_ForcedTypeIdx = p_Preferences.forceMemoryIndex;
    
    uint32_t l_Visible;
    if (l_ForcedTypeIdx != UINT32_MAX) 
    {
        l_Visible = (1u << l_ForcedTypeIdx);
    }
    else 
    {
        const uint32_t l_Idx = findMemoryType(l_Reqs, p_Preferences);
        if (l_Idx == UINT32_MAX) return {};
        l_Visible = (1u << l_Idx);
    }
    
    VmaAllocation l_Alloc{};
    VmaAllocationInfo l_Info{};
    const VmaAllocationCreateInfo l_Aci = toVmaAllocCI(p_Preferences, l_Visible);
    VULKAN_TRY(vmaAllocateMemoryForImage(m_Allocator, *l_Image, &l_Aci, &l_Alloc, &l_Info));
    VULKAN_TRY(vmaBindImageMemory(m_Allocator, l_Alloc, *l_Image));
    return l_Alloc;
}

uint32_t VulkanMemoryAllocator::getOrCreatePool(const PoolPreferences& p_Prefs)
{
    for (const PoolData& l_Pool : m_Pools)
    {
        if (l_Pool.prefs == p_Prefs)
        {
            return l_Pool.id;
        }
    }
    return createPool(p_Prefs);
}

uint32_t VulkanMemoryAllocator::createPool(const PoolPreferences& p_Prefs)
{
    static uint32_t l_NewID = 0;

    const VmaPoolCreateInfo l_Pci{
        .memoryTypeIndex = p_Prefs.memoryTypeIndex,
        .flags = p_Prefs.flags,
        .blockSize = p_Prefs.blockSize,
        .minBlockCount = p_Prefs.minBlockCount,
        .maxBlockCount = p_Prefs.maxBlockCount,
        .priority = p_Prefs.priority,
        .minAllocationAlignment = p_Prefs.customMinAlignment,
        .pMemoryAllocateNext = p_Prefs.pNext
    };

    VmaPool l_Pool;
    VULKAN_TRY(vmaCreatePool(m_Allocator, &l_Pci, &l_Pool));
    m_Pools.push_back({l_NewID++, l_Pool, p_Prefs});
    m_Pools.back().prefs.pNext = m_Pools.back().prefs.pNext == nullptr ? nullptr : reinterpret_cast<void*>(UINT64_MAX);
    LOG_DEBUG("Created memory pool with ID ", m_Pools.back().id, " for memory type ", p_Prefs.memoryTypeIndex);
    return m_Pools.back().id;
}

void* VulkanMemoryAllocator::map(const VmaAllocation p_Alloc) const
{
    void* l_Data = nullptr;
    VULKAN_TRY(vmaMapMemory(m_Allocator, p_Alloc, &l_Data));
    return l_Data;
}

void VulkanMemoryAllocator::unmap(const VmaAllocation p_Alloc) const
{
    vmaUnmapMemory(m_Allocator, p_Alloc);
}

void VulkanMemoryAllocator::deallocate(const VmaAllocation p_Alloc) const
{
    vmaFreeMemory(m_Allocator, p_Alloc);
}

const MemoryStructure& VulkanMemoryAllocator::getMemoryStructure() const
{
    return m_MemoryStructure;
}

VmaAllocationInfo VulkanMemoryAllocator::getAllocationInfo(const VmaAllocation p_Allocation) const
{
    VmaAllocationInfo l_Info{};
    vmaGetAllocationInfo(m_Allocator, p_Allocation, &l_Info);
    return l_Info;
}

VulkanMemoryAllocator::VulkanMemoryAllocator(const VulkanDevice& p_Device)
    : m_MemoryStructure(p_Device.getGPU()), m_Device(p_Device.getID())
{
    VmaVulkanFunctions l_Funcs{};
    l_Funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    l_Funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo l_AllocInfo = {};
    l_AllocInfo.instance = VulkanContext::getHandle();
    l_AllocInfo.physicalDevice = **m_MemoryStructure;
    l_AllocInfo.device = *p_Device;
    l_AllocInfo.vulkanApiVersion = VK_HEADER_VERSION_COMPLETE;
    l_AllocInfo.pVulkanFunctions = &l_Funcs;

    VULKAN_TRY(vmaCreateAllocator(&l_AllocInfo, &m_Allocator));
}

VulkanMemoryAllocator::AllocationReturn VulkanMemoryAllocator::createBuffer(const VkBufferCreateInfo& p_Info, const MemoryPreferences& p_Preferences) const
{
    const VmaAllocationCreateInfo l_Aci = toVmaAllocCI(p_Preferences, 0);
    VkBuffer l_Buffer;
    VmaAllocation l_Alloc;
    VmaAllocationInfo l_Info;
    VULKAN_TRY(vmaCreateBuffer(m_Allocator, &p_Info, &l_Aci, &l_Buffer, &l_Alloc, &l_Info));
    LOG_DEBUG("Created buffer with size ", VulkanMemoryAllocator::compactBytes(l_Info.size), " at memory type ", l_Info.memoryType, " with offset ", l_Info.offset, ". Handle ", reinterpret_cast<void*>(l_Alloc));
    return { reinterpret_cast<uintptr_t>(l_Buffer), l_Alloc };
}

VulkanMemoryAllocator::AllocationReturn VulkanMemoryAllocator::createImage(const VkImageCreateInfo& p_Info, const MemoryPreferences& p_Preferences) const
{
    const VmaAllocationCreateInfo l_Aci = toVmaAllocCI(p_Preferences, 0);
    VkImage l_Image;
    VmaAllocation l_Alloc;
    VmaAllocationInfo l_Info;
    VULKAN_TRY(vmaCreateImage(m_Allocator, &p_Info, &l_Aci, &l_Image, &l_Alloc, &l_Info));
    LOG_DEBUG("Created image with size ", VulkanMemoryAllocator::compactBytes(l_Info.size), " at memory type ", l_Info.memoryType, " with offset ", l_Info.offset, ". Handle ", reinterpret_cast<void*>(l_Alloc));
    return { reinterpret_cast<uintptr_t>(l_Image), l_Alloc };
}

VmaAllocationCreateInfo VulkanMemoryAllocator::toVmaAllocCI(const MemoryPreferences& p_Preferences, const uint32_t p_MemoryTypeBits) const
{
    VmaAllocationCreateInfo l_Aci{};
    l_Aci.usage = p_Preferences.usage;
    l_Aci.requiredFlags = p_Preferences.desiredProperties;
    l_Aci.preferredFlags = p_Preferences.preferredProperties;
    l_Aci.memoryTypeBits = p_MemoryTypeBits;
    l_Aci.pool = p_Preferences.pool < m_Pools.size() ? getPool(p_Preferences.pool) : VK_NULL_HANDLE;
    l_Aci.flags = p_Preferences.vmaFlags;

    if (VulkanContext::getDevice(m_Device).isExtensionEnabled(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME))
        l_Aci.priority = p_Preferences.priority;

    return l_Aci;
}

VmaPool VulkanMemoryAllocator::getPool(const uint32_t p_Id) const
{
    for (const PoolData& l_Pool : m_Pools)
    {
        if (l_Pool.id == p_Id)
        {
            return l_Pool.pool;
        }
    }
    LOG_WARN("Tried to get memory pool with ID ", p_Id, " but it doesn't exist");
    return VK_NULL_HANDLE;
}
