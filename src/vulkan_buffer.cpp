#include "vulkan_buffer.hpp"

#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#include "utils/logger.hpp"
#include "vulkan_base.hpp"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"

void VulkanMemArray::allocate(MemoryPreferences p_Preferences)
{
    setBoundMemory(VulkanContext::getDevice(getDeviceID()).getMemoryAllocator().allocateMemArray(getID(), p_Preferences));
}

bool VulkanMemArray::isMemoryBound() const
{
    return m_Allocation != VK_NULL_HANDLE;
}

uint32_t VulkanMemArray::getBoundMemoryType() const
{
    if (!m_Allocation)
    {
        throw std::runtime_error("Buffer (ID:" + std::to_string(m_ID) + ") does not have memory bound to it!");
    }
    return VulkanContext::getDevice(getDeviceID()).getMemoryAllocator().getAllocationInfo(m_Allocation).memoryType;
}

VkDeviceSize VulkanMemArray::getMemorySize() const
{
    if (!m_Allocation)
    {
        throw std::runtime_error("Buffer (ID:" + std::to_string(m_ID) + ") does not have memory bound to it!");
    }
    return VulkanContext::getDevice(getDeviceID()).getMemoryAllocator().getAllocationInfo(m_Allocation).size;
}

VmaAllocation VulkanMemArray::getAllocation() const
{
    return m_Allocation;
}

bool VulkanMemArray::isMemoryMapped() const
{
    return m_MappedData != nullptr;
}

void* VulkanMemArray::getMappedData() const
{
    return m_MappedData;
}

VkDeviceAddress VulkanBuffer::getDeviceAddress() const
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkBufferDeviceAddressInfo l_Info = {};
    l_Info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    l_Info.buffer = m_VkHandle;
    return l_Device.getTable().vkGetBufferDeviceAddress(*l_Device, &l_Info);
}

void* VulkanMemArray::map(const VkDeviceSize p_Size, const VkDeviceSize p_Offset)
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    void* l_Data = l_Device.getMemoryAllocator().map(m_Allocation);
    LOG_DEBUG("Mapped buffer (ID:", m_ID, ") memory with size ", VulkanMemoryAllocator::compactBytes(p_Size), " and offset ", p_Offset);
    m_MappedData = l_Data;
    return l_Data;
}

VkMemoryRequirements VulkanBuffer::getMemoryRequirements() const
{
    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    VkMemoryRequirements l_MemoryRequirements;
    l_Device.getTable().vkGetBufferMemoryRequirements(l_Device.m_VkHandle, m_VkHandle, &l_MemoryRequirements);
    return l_MemoryRequirements;
}

void VulkanBuffer::allocate(const MemoryPreferences p_Preferences)
{
    Logger::pushContext("Buffer memory");
    VulkanMemArray::allocate(p_Preferences);
    m_Size = m_Allocation ? VulkanContext::getDevice(getDeviceID()).getMemoryAllocator().getAllocationInfo(m_Allocation).size : 0;
    Logger::popContext();
}

VkDeviceSize VulkanBuffer::getSize() const
{
    return m_Size;
}

uint32_t VulkanBuffer::getQueue() const
{
    return m_QueueFamilyIndex;
}

void VulkanBuffer::setQueue(const uint32_t p_QueueFamilyIndex)
{
    m_QueueFamilyIndex = p_QueueFamilyIndex;
}

VkBuffer VulkanBuffer::operator*() const
{
    return m_VkHandle;
}

void VulkanMemArray::unmap()
{
    if (!isMemoryMapped())
    {
        LOG_WARN("Tried to unmap memory for buffer (ID:", m_ID, "), but memory was not mapped");
        return;
    }

    const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    l_Device.getMemoryAllocator().unmap(m_Allocation);
    LOG_DEBUG("Unmapped buffer (ID:", m_ID, ") memory");
    m_MappedData = nullptr;
}

VulkanBuffer::VulkanBuffer(const uint32_t p_Device, const VkBuffer p_VkHandle, const VkDeviceSize p_Size)
    : VulkanMemArray(p_Device), m_VkHandle(p_VkHandle), m_Size(p_Size) {}

void VulkanBuffer::setBoundMemory(VmaAllocation p_Allocation)
{
    m_Allocation = p_Allocation;
}

void VulkanBuffer::free()
{
    VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

    l_Device.getTable().vkDestroyBuffer(*l_Device, m_VkHandle, nullptr);
    LOG_DEBUG("Freed buffer (ID:", m_ID, ")");
    m_VkHandle = VK_NULL_HANDLE;
    
    if (m_Allocation)
    {
        if (isMemoryMapped())
            unmap();

        Logger::pushContext("Buffer memory free");
        l_Device.getMemoryAllocator().deallocate(m_Allocation);
        m_Allocation = VK_NULL_HANDLE;
        Logger::popContext();
    }
}