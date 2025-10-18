#pragma once
#include <optional>
#include <string>
#include <vector>

#include "vulkan_gpu.hpp"
#include "utils/identifiable.hpp"

#include <vma/vk_mem_alloc.h>
#include <bitset>

class VulkanDevice;

class MemoryStructure
{
public:
    struct MemoryTypeData
    {
        VkMemoryPropertyFlags properties;
        uint32_t heapIndex;
        VkDeviceSize heapSize;
    };

    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const;

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] std::optional<uint32_t> getStagingMemoryType(uint32_t p_TypeFilter) const;
    [[nodiscard]] std::vector<uint32_t> getMemoryTypes(VkMemoryPropertyFlags p_Properties, uint32_t p_TypeFilter) const;
    [[nodiscard]] bool doesMemoryContainProperties(uint32_t p_Type, VkMemoryPropertyFlags p_Property) const;
    [[nodiscard]] MemoryTypeData getTypeData(uint32_t p_MemoryType) const;
    [[nodiscard]] VkMemoryHeap getMemoryTypeHeap(uint32_t p_MemoryType) const;
    [[nodiscard]] uint32_t getMemoryTypeCount() const;
    [[nodiscard]] uint32_t getMemoryHeapCount() const;

    VulkanGPU operator*() const { return m_GPU; }

private:
    MemoryStructure() = default;
    explicit MemoryStructure(VulkanGPU p_GPU);

    VkPhysicalDeviceMemoryProperties m_MemoryProperties{};

    VulkanGPU m_GPU;

    friend class VulkanMemoryAllocator;
};

class VulkanMemoryAllocator
{
public:
    struct AllocationResult
    {
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo info{};
    };

    struct MemoryPreferences
    {
        VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO;
        VmaAllocationCreateFlags vmaFlags = 0;
        uint32_t pool = UINT32_MAX;
        float priority = 1.0f;

        uint32_t forceMemoryIndex = UINT32_MAX;

        VkMemoryPropertyFlags desiredProperties = 0;
        VkMemoryPropertyFlags preferredProperties = 0;

        static MemoryPreferences fromDefault() { return {}; }
        static MemoryPreferences fromIndex(const uint32_t p_Index) { return { .forceMemoryIndex = p_Index }; }
        static MemoryPreferences fromUsage(VmaMemoryUsage p_Usage, VmaAllocationCreateFlags p_Flags);
    };

    struct PoolPreferences
    {
        uint32_t memoryTypeIndex = UINT32_MAX;
        VmaPoolCreateFlags flags = 0;

        VkDeviceSize blockSize = 0;
        uint32_t minBlockCount = 0;
        uint32_t maxBlockCount = 0;

        float priority = 1.0f;

        size_t customMinAlignment = 0;
        void* pNext = nullptr;
        uint64_t pNextIdentifier = 0;

        bool operator==(const PoolPreferences& p_Other) const;
    };

    [[nodiscard]] uint32_t findMemoryType(const MemoryPreferences& p_Preferences) const;
    [[nodiscard]] uint32_t findMemoryType(const VkMemoryRequirements& p_Reqs, const MemoryPreferences& p_Preferences) const;
    [[nodiscard]] uint32_t findMemoryType(const MemoryPreferences& p_Preferences, uint32_t p_StartingFilter) const;

    [[nodiscard]] VmaAllocation allocateMemArray(ResourceID p_MemArray, const MemoryPreferences& p_Preferences) const;
    [[nodiscard]] VmaAllocation allocateBuffer(ResourceID p_Buffer, const MemoryPreferences& p_Preferences) const;
    [[nodiscard]] VmaAllocation allocateImage(ResourceID p_Image, const MemoryPreferences& p_Preferences) const;

    uint32_t getOrCreatePool(const PoolPreferences& p_Prefs);
    uint32_t createPool(const PoolPreferences& p_Prefs);

    void* map(VmaAllocation p_Alloc) const;
    void unmap(VmaAllocation p_Alloc) const;
    void deallocate(VmaAllocation p_Alloc) const;

    [[nodiscard]] const MemoryStructure& getMemoryStructure() const;
    [[nodiscard]] VmaAllocationInfo getAllocationInfo(VmaAllocation p_Allocation) const;

    VmaAllocator operator*() const { return m_Allocator; }

    static std::string compactBytes(VkDeviceSize p_Bytes);

private:
    bool free();

    struct AllocationReturn
    {
        uintptr_t vkObj;
        VmaAllocation allocation;

        template <typename T>
        T as() const { return reinterpret_cast<T>(vkObj); }
    };

    struct PoolData
    {
        uint32_t id;

        VmaPool pool;
        PoolPreferences prefs;
    };

    VulkanMemoryAllocator() = default;
    explicit VulkanMemoryAllocator(const VulkanDevice& p_Device);

    [[nodiscard]] AllocationReturn createBuffer(const VkBufferCreateInfo& p_Info, const MemoryPreferences& p_Preferences) const;
    [[nodiscard]] AllocationReturn createImage(const VkImageCreateInfo& p_Info, const MemoryPreferences& p_Preferences) const;

    [[nodiscard]] VmaAllocationCreateInfo toVmaAllocCI(const MemoryPreferences& p_Preferences, uint32_t p_MemoryTypeBits) const;

    [[nodiscard]] VmaPool getPool(uint32_t p_Id) const;

    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    MemoryStructure m_MemoryStructure{};

    std::vector<PoolData> m_Pools{};

    ResourceID m_Device;

    friend class VulkanDevice;
};