#pragma once
#include <span>
#include <string>
#include <vector>
#include <Volk/volk.h>

#include "utils/identifiable.hpp"
#include "vulkan_binding.hpp"

class ShaderHash;
class VulkanDevice;

struct VulkanPipelineBuilder
{
    explicit VulkanPipelineBuilder(ResourceID p_Device);
    
    void addShaderStage(ResourceID p_Shader, std::string_view p_Entrypoint = "main");
    void resetShaderStages();

    void setVertexInputState(const VkPipelineVertexInputStateCreateInfo& p_State);
    void addVertexBinding(const VulkanBinding& p_Binding, bool p_RecalculateLocations = false);

    void setInputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& p_State);
    void setInputAssemblyState(VkPrimitiveTopology p_Topology, VkBool32 p_PrimitiveRestartEnable);

    void setTessellationState(const VkPipelineTessellationStateCreateInfo& p_State);
    void setTessellationState(uint32_t p_PatchControlPoints);

    void setViewportState(const VkPipelineViewportStateCreateInfo& p_State);
    void setViewportState(uint32_t p_ViewportCount, uint32_t p_ScissorCount);
    void setViewportState(std::span<const VkViewport> p_Viewports, std::span<const VkRect2D> p_Scissors);

    void setRasterizationState(const VkPipelineRasterizationStateCreateInfo& p_State);
    void setRasterizationState(VkPolygonMode p_PolygonMode, VkCullModeFlags p_CullMode = VK_CULL_MODE_BACK_BIT, VkFrontFace p_FrontFace = VK_FRONT_FACE_CLOCKWISE);

    void setMultisampleState(const VkPipelineMultisampleStateCreateInfo& p_State);
    void setMultisampleState(VkSampleCountFlagBits p_RasterizationSamples, VkBool32 p_SampleShadingEnable = VK_FALSE, float p_MinSampleShading = 1.0f);

    void setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& p_State);
    void setDepthStencilState(VkBool32 p_DepthTestEnable, VkBool32 p_DepthWriteEnable, VkCompareOp p_DepthCompareOp);

    void setColorBlendState(const VkPipelineColorBlendStateCreateInfo& p_State);
    void setColorBlendState(VkBool32 p_LogicOpEnable, VkLogicOp p_LogicOp, std::array<float, 4> p_ColorBlendConstants);
    void addColorBlendAttachment(const VkPipelineColorBlendAttachmentState& p_Attachment);

    void setDynamicState(const VkPipelineDynamicStateCreateInfo& p_State);
    void setDynamicState(std::span<VkDynamicState> p_DynamicStates);

    [[nodiscard]] size_t getShaderStageCount() const { return m_ShaderStages.size(); }

    [[nodiscard]] size_t getHash(size_t p_Hash) const;

private:
    VkPipelineVertexInputStateCreateInfo m_VertexInputState{};
    VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState{};
    VkPipelineTessellationStateCreateInfo m_TessellationState{};
    VkPipelineViewportStateCreateInfo m_ViewportState{};
    VkPipelineRasterizationStateCreateInfo m_RasterizationState{};
    VkPipelineMultisampleStateCreateInfo m_MultisampleState{};
    VkPipelineDepthStencilStateCreateInfo m_DepthStencilState{};
    VkPipelineColorBlendStateCreateInfo m_ColorBlendState{};
    VkPipelineDynamicStateCreateInfo m_DynamicState{};

    bool m_TesellationStateEnabled = false;

    [[nodiscard]] size_t getVertexInputHash() const;
    [[nodiscard]] size_t getInputAssemblyHash() const;
    [[nodiscard]] size_t getTessellationHash() const;
    [[nodiscard]] size_t getViewportStateHash() const;
    [[nodiscard]] size_t getRasterizationHash() const;
    [[nodiscard]] size_t getMultisampleHash() const;
    [[nodiscard]] size_t getDepthStencilHash() const;
    [[nodiscard]] size_t getColorBlendHash() const;
    [[nodiscard]] size_t getDynamicStateHash() const;

    struct ShaderData
    {
        ResourceID shader;
        std::string entrypoint;

        ShaderData(const ResourceID p_Shader, const std::string_view p_Entrypoint) : shader(p_Shader), entrypoint(p_Entrypoint) {}
        explicit ShaderData(const ResourceID p_Shader) : shader(p_Shader), entrypoint("main") {}
    };

    std::vector<ShaderData> m_ShaderStages;
    std::vector<VkVertexInputBindingDescription> m_VertexInputBindings;
    std::vector<VkVertexInputAttributeDescription> m_VertexInputAttributes;
    uint32_t m_CurrentVertexAttrLocation = 0;
    std::vector<VkViewport> m_Viewports;
    std::vector<VkRect2D> m_Scissors;
    std::vector<VkPipelineColorBlendAttachmentState> m_Attachments;
    std::vector<VkDynamicState> m_DynamicStates;

    ResourceID m_Device;

    void createShaderStages(VkPipelineShaderStageCreateInfo p_Container[]) const;

    friend class VulkanDevice;
};

class VulkanPipeline final : public VulkanDeviceSubresource
{
public:
    [[nodiscard]] ResourceID getLayout() const;
    [[nodiscard]] ResourceID getRenderPass() const;
    [[nodiscard]] ResourceID getSubpass() const;

    VkPipeline operator*() const;

private:
    void free() override;

    VulkanPipeline(ResourceID p_Device, VkPipeline p_Handle, ResourceID p_Layout, ResourceID p_RenderPass, ResourceID p_Subpass);

    VkPipeline m_VkHandle;

    ResourceID m_Layout;
    ResourceID m_RenderPass;
    ResourceID m_Subpass;

    friend class VulkanDevice;
    friend class VulkanCommandBuffer;
};

class VulkanPipelineLayout final : public VulkanDeviceSubresource
{
public:
    VkPipelineLayout operator*() const;

private:
    void free() override;

    VulkanPipelineLayout(uint32_t p_Device, VkPipelineLayout p_Handle, size_t p_Hash = 0);

    VkPipelineLayout m_VkHandle = VK_NULL_HANDLE;

    size_t m_Hash = 0;

    friend class VulkanDevice;
    friend class VulkanCommandBuffer;
};

class VulkanComputePipeline final : public VulkanDeviceSubresource
{
public:
    VkPipeline operator*() const { return m_VkHandle; }

private:
    void free() override;

    VulkanComputePipeline(ResourceID p_Device, VkPipeline p_Handle);

    VkPipeline m_VkHandle;

    friend class VulkanDevice;
    friend class VulkanCommandBuffer;
};
