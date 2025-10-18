#include "vulkan_pipeline.hpp"

#include <array>

#include "shader_helper.hpp"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"
#include "vulkan_shader.hpp"
#include "utils/logger.hpp"


VulkanPipelineBuilder::VulkanPipelineBuilder(const ResourceID p_Device)
    : m_Device(p_Device)
{
    m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_VertexInputState.vertexBindingDescriptionCount = 0;
    m_VertexInputState.vertexAttributeDescriptionCount = 0;

    m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_InputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_InputAssemblyState.primitiveRestartEnable = VK_FALSE;

    m_TessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    m_TessellationState.patchControlPoints = 1;
    m_TesellationStateEnabled = false;

    m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_ViewportState.viewportCount = 1;
    m_ViewportState.scissorCount = 1;

    m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_RasterizationState.depthClampEnable = VK_FALSE;
    m_RasterizationState.rasterizerDiscardEnable = VK_FALSE;
    m_RasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    m_RasterizationState.lineWidth = 1.0f;
    m_RasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    m_RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_RasterizationState.depthBiasEnable = VK_FALSE;

    m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_MultisampleState.sampleShadingEnable = VK_FALSE;
    m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_DepthStencilState.depthTestEnable = VK_TRUE;
    m_DepthStencilState.depthWriteEnable = VK_TRUE;
    m_DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    m_DepthStencilState.depthBoundsTestEnable = VK_FALSE;
    m_DepthStencilState.stencilTestEnable = VK_FALSE;

    m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_ColorBlendState.logicOpEnable = VK_FALSE;
    m_ColorBlendState.logicOp = VK_LOGIC_OP_COPY;
    m_ColorBlendState.attachmentCount = 0;
    m_ColorBlendState.blendConstants[0] = 0.0f;
    m_ColorBlendState.blendConstants[1] = 0.0f;
    m_ColorBlendState.blendConstants[2] = 0.0f;
    m_ColorBlendState.blendConstants[3] = 0.0f;

    m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_DynamicState.dynamicStateCount = 0;
}

void VulkanPipelineBuilder::addVertexBinding(const VulkanBinding& p_Binding, bool p_RecalculateLocations)
{
    m_VertexInputBindings.push_back(p_Binding.getBindingDescription());
    TRANS_VECTOR(l_Attributes, VkVertexInputAttributeDescription);
    l_Attributes.resize(p_Binding.getAttributeDescriptionCount());
    p_Binding.getAttributeDescriptions(l_Attributes.data());
    for (VkVertexInputAttributeDescription& l_Attr : l_Attributes)
    {
        m_VertexInputAttributes.push_back(l_Attr);
        if (p_RecalculateLocations)
        {
            m_VertexInputAttributes.back().location = m_CurrentVertexAttrLocation;
            m_CurrentVertexAttrLocation += l_Attr.location;
        }
    }
    m_VertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_VertexInputBindings.size());
    m_VertexInputState.pVertexBindingDescriptions = m_VertexInputBindings.data();
    m_VertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_VertexInputAttributes.size());
    m_VertexInputState.pVertexAttributeDescriptions = m_VertexInputAttributes.data();
}

void VulkanPipelineBuilder::setInputAssemblyState(const VkPrimitiveTopology p_Topology, const VkBool32 p_PrimitiveRestartEnable)
{
    m_InputAssemblyState.topology = p_Topology;
    m_InputAssemblyState.primitiveRestartEnable = p_PrimitiveRestartEnable;
}

void VulkanPipelineBuilder::setTessellationState(const uint32_t p_PatchControlPoints)
{
    m_TessellationState.patchControlPoints = p_PatchControlPoints;
    m_TesellationStateEnabled = true;
}

void VulkanPipelineBuilder::setViewportState(const uint32_t p_ViewportCount, const uint32_t p_ScissorCount)
{
    m_ViewportState.viewportCount = p_ViewportCount;
    m_ViewportState.scissorCount = p_ScissorCount;
}

void VulkanPipelineBuilder::setViewportState(const std::span<const VkViewport> p_Viewports, const std::span<const VkRect2D> p_Scissors)
{
    m_Viewports = std::vector<VkViewport>(p_Viewports.begin(), p_Viewports.end());
    m_Scissors = std::vector<VkRect2D>(p_Scissors.begin(), p_Scissors.end());
    m_ViewportState.viewportCount = static_cast<uint32_t>(m_Viewports.size());
    m_ViewportState.pViewports = m_Viewports.data();
    m_ViewportState.scissorCount = static_cast<uint32_t>(m_Scissors.size());
    m_ViewportState.pScissors = m_Scissors.data();
}

void VulkanPipelineBuilder::setRasterizationState(const VkPolygonMode p_PolygonMode, const VkCullModeFlags p_CullMode, const VkFrontFace p_FrontFace)
{
    m_RasterizationState.polygonMode = p_PolygonMode;
    m_RasterizationState.cullMode = p_CullMode;
    m_RasterizationState.frontFace = p_FrontFace;
}

void VulkanPipelineBuilder::setMultisampleState(const VkSampleCountFlagBits p_RasterizationSamples, const VkBool32 p_SampleShadingEnable, const float p_MinSampleShading)
{
    m_MultisampleState.rasterizationSamples = p_RasterizationSamples;
    m_MultisampleState.sampleShadingEnable = p_SampleShadingEnable;
    m_MultisampleState.minSampleShading = p_MinSampleShading;
}

void VulkanPipelineBuilder::setDepthStencilState(const VkBool32 p_DepthTestEnable, const VkBool32 p_DepthWriteEnable, const VkCompareOp p_DepthCompareOp)
{
    m_DepthStencilState.depthTestEnable = p_DepthTestEnable;
    m_DepthStencilState.depthWriteEnable = p_DepthWriteEnable;
    m_DepthStencilState.depthCompareOp = p_DepthCompareOp;
}

void VulkanPipelineBuilder::setColorBlendState(const VkBool32 p_LogicOpEnable, const VkLogicOp p_LogicOp, const std::array<float, 4> p_ColorBlendConstants)
{
    m_ColorBlendState.logicOpEnable = p_LogicOpEnable;
    m_ColorBlendState.logicOp = p_LogicOp;
    m_ColorBlendState.blendConstants[0] = p_ColorBlendConstants[0];
    m_ColorBlendState.blendConstants[1] = p_ColorBlendConstants[1];
    m_ColorBlendState.blendConstants[2] = p_ColorBlendConstants[2];
    m_ColorBlendState.blendConstants[3] = p_ColorBlendConstants[3];
}

void VulkanPipelineBuilder::addColorBlendAttachment(const VkPipelineColorBlendAttachmentState& p_Attachment)
{
    m_Attachments.push_back(p_Attachment);
    m_ColorBlendState.attachmentCount = static_cast<uint32_t>(m_Attachments.size());
    m_ColorBlendState.pAttachments = m_Attachments.data();
}

void VulkanPipelineBuilder::setDynamicState(const std::span<VkDynamicState> p_DynamicStates)
{
    m_DynamicStates = std::vector<VkDynamicState>(p_DynamicStates.begin(), p_DynamicStates.end());
    m_DynamicState.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
    m_DynamicState.pDynamicStates = m_DynamicStates.data();
}

size_t VulkanPipelineBuilder::getHash(const size_t p_Hash) const
{
    size_t l_Hash = 0;
    hashCombine(l_Hash, p_Hash);
    for (const ShaderData& l_ShaderStage : m_ShaderStages)
    {
        const VulkanShaderModule& l_Shader = VulkanContext::getDevice(m_Device).getShaderModule(l_ShaderStage.shader);
        hashCombine(l_Hash, l_Shader.getStage());
        hashCombine(l_Hash, l_ShaderStage.entrypoint);
    }
    hashCombine(l_Hash, getVertexInputHash());
    hashCombine(l_Hash, getInputAssemblyHash());
    if (m_TesellationStateEnabled)
        hashCombine(l_Hash, getTessellationHash());
    hashCombine(l_Hash, getViewportStateHash());
    hashCombine(l_Hash, getRasterizationHash());
    hashCombine(l_Hash, getMultisampleHash());
    hashCombine(l_Hash, getDepthStencilHash());
    hashCombine(l_Hash, getColorBlendHash());
    hashCombine(l_Hash, getDynamicStateHash());

    return l_Hash;
}

size_t VulkanPipelineBuilder::getVertexInputHash() const
{
    size_t l_Hash;
    if (m_VertexInputState.pNext)
    {
        const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_VertexInputState.pNext);
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineVertexInputDivisorStateCreateInfoEXT* l_DivisorInfo = reinterpret_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(l_PNext);
                for (uint32_t i = 0; i < l_DivisorInfo->vertexBindingDivisorCount; i++)
                {
                    hashCombine(l_Hash, l_DivisorInfo->pVertexBindingDivisors[i].binding);
                    hashCombine(l_Hash, l_DivisorInfo->pVertexBindingDivisors[i].divisor);
                }
                break;
            }
        default:
            break;
        }
    }
    hashCombine(l_Hash, m_VertexInputState.flags);
    for (uint32_t i = 0; i < m_VertexInputState.vertexBindingDescriptionCount; i++)
    {
        const VkVertexInputBindingDescription& l_Binding = m_VertexInputState.pVertexBindingDescriptions[i];
        hashCombine(l_Hash, l_Binding.binding);
        hashCombine(l_Hash, l_Binding.stride);
        hashCombine(l_Hash, l_Binding.inputRate);
    }
    for (uint32_t i = 0; i < m_VertexInputState.vertexAttributeDescriptionCount; i++)
    {
        const VkVertexInputAttributeDescription& l_Attr = m_VertexInputState.pVertexAttributeDescriptions[i];
        hashCombine(l_Hash, l_Attr.location);
        hashCombine(l_Hash, l_Attr.binding);
        hashCombine(l_Hash, l_Attr.format);
        hashCombine(l_Hash, l_Attr.offset);
    }

    return l_Hash;
}

size_t VulkanPipelineBuilder::getInputAssemblyHash() const
{
    size_t l_Hash;
    hashCombine(l_Hash, m_InputAssemblyState.flags);
    hashCombine(l_Hash, m_InputAssemblyState.topology);
    hashCombine(l_Hash, m_InputAssemblyState.primitiveRestartEnable);
    return l_Hash;
}

size_t VulkanPipelineBuilder::getTessellationHash() const
{
    size_t l_Hash;
    if (m_TessellationState.pNext)
    {
        const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_TessellationState.pNext);
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
            {
                const VkPipelineTessellationDomainOriginStateCreateInfo* l_DomainOriginInfo = reinterpret_cast<const VkPipelineTessellationDomainOriginStateCreateInfo*>(l_PNext);
                hashCombine(l_Hash, l_DomainOriginInfo->domainOrigin);
                break;
            }
        default:
            break;
        }
    }
    hashCombine(l_Hash, m_TessellationState.flags);
    hashCombine(l_Hash, m_TessellationState.patchControlPoints);
    return l_Hash;
}

size_t VulkanPipelineBuilder::getViewportStateHash() const
{
    size_t l_Hash;
    const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_ViewportState.pNext);
    while (l_PNext)
    {
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
            {
                const VkPipelineTessellationDomainOriginStateCreateInfo* l_DomainOriginInfo = reinterpret_cast<const VkPipelineTessellationDomainOriginStateCreateInfo*>(l_PNext);
                hashCombine(l_Hash, l_DomainOriginInfo->domainOrigin);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV:
            {
                const VkPipelineViewportCoarseSampleOrderStateCreateInfoNV* l_SampleOrderInfo = reinterpret_cast<const VkPipelineViewportCoarseSampleOrderStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_SampleOrderInfo->sampleOrderType);
                for (uint32_t i = 0; i < l_SampleOrderInfo->customSampleOrderCount; i++)
                {
                    const VkCoarseSampleOrderCustomNV& l_CustomOrder = l_SampleOrderInfo->pCustomSampleOrders[i];
                    hashCombine(l_Hash, l_CustomOrder.shadingRate);
                    hashCombine(l_Hash, l_CustomOrder.sampleCount);
                    for (uint32_t j = 0; j < l_CustomOrder.sampleLocationCount; j++)
                    {
                        const VkCoarseSampleLocationNV& l_Location = l_CustomOrder.pSampleLocations[j];
                        hashCombine(l_Hash, l_Location.pixelX);
                        hashCombine(l_Hash, l_Location.pixelY);
                        hashCombine(l_Hash, l_Location.sample);
                    }
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLAMP_CONTROL_CREATE_INFO_EXT:
            {
                const VkPipelineViewportDepthClampControlCreateInfoEXT* l_DepthClampInfo = reinterpret_cast<const VkPipelineViewportDepthClampControlCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_DepthClampInfo->depthClampMode);
                if (l_DepthClampInfo->pDepthClampRange)
                {
                    hashCombine(l_Hash, l_DepthClampInfo->pDepthClampRange->minDepthClamp);
                    hashCombine(l_Hash, l_DepthClampInfo->pDepthClampRange->maxDepthClamp);
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT:
            {
                const VkPipelineViewportDepthClipControlCreateInfoEXT* l_DepthClipInfo = reinterpret_cast<const VkPipelineViewportDepthClipControlCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_DepthClipInfo->negativeOneToOne);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV:
            {
                const VkPipelineViewportExclusiveScissorStateCreateInfoNV* l_ExclusiveScissorInfo = reinterpret_cast<const VkPipelineViewportExclusiveScissorStateCreateInfoNV*>(l_PNext);
                for (uint32_t i = 0; i < l_ExclusiveScissorInfo->exclusiveScissorCount; i++)
                {
                    const VkRect2D& l_Scissor = l_ExclusiveScissorInfo->pExclusiveScissors[i];
                    hashCombine(l_Hash, l_Scissor.offset.x);
                    hashCombine(l_Hash, l_Scissor.offset.y);
                    hashCombine(l_Hash, l_Scissor.extent.width);
                    hashCombine(l_Hash, l_Scissor.extent.height);
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV:
            {
                const VkPipelineViewportShadingRateImageStateCreateInfoNV* l_ShadingRateInfo = reinterpret_cast<const VkPipelineViewportShadingRateImageStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_ShadingRateInfo->shadingRateImageEnable);
                for (uint32_t i = 0; i < l_ShadingRateInfo->viewportCount; i++)
                {
                    const VkShadingRatePaletteNV& l_Palette = l_ShadingRateInfo->pShadingRatePalettes[i];
                    for (uint32_t j = 0; j < l_Palette.shadingRatePaletteEntryCount; j++)
                    {
                        hashCombine(l_Hash, l_Palette.pShadingRatePaletteEntries[j]);
                    }
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV:
            {
                const VkPipelineViewportSwizzleStateCreateInfoNV* l_SwizzleInfo = reinterpret_cast<const VkPipelineViewportSwizzleStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_SwizzleInfo->flags);
                for (uint32_t i = 0; i < l_SwizzleInfo->viewportCount; i++)
                {
                    const VkViewportSwizzleNV& l_Swizzle = l_SwizzleInfo->pViewportSwizzles[i];
                    hashCombine(l_Hash, l_Swizzle.x);
                    hashCombine(l_Hash, l_Swizzle.y);
                    hashCombine(l_Hash, l_Swizzle.z);
                    hashCombine(l_Hash, l_Swizzle.w);
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV:
            {
                const VkPipelineViewportWScalingStateCreateInfoNV* l_WScalingInfo = reinterpret_cast<const VkPipelineViewportWScalingStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_WScalingInfo->viewportWScalingEnable);
                for (uint32_t i = 0; i < l_WScalingInfo->viewportCount; i++)
                {
                    const VkViewportWScalingNV& l_WScaling = l_WScalingInfo->pViewportWScalings[i];
                    hashCombine(l_Hash, l_WScaling.xcoeff);
                    hashCombine(l_Hash, l_WScaling.ycoeff);
                }
                break;
            }
        default:
            break;
        }
        l_PNext = l_PNext->pNext;
    }
    hashCombine(l_Hash, m_ViewportState.flags);
    hashCombine(l_Hash, m_ViewportState.viewportCount);
    for (uint32_t i = 0; i < m_ViewportState.viewportCount; i++)
    {
        const VkViewport& l_Viewport = m_ViewportState.pViewports[i];
        hashCombine(l_Hash, l_Viewport.x);
        hashCombine(l_Hash, l_Viewport.y);
        hashCombine(l_Hash, l_Viewport.width);
        hashCombine(l_Hash, l_Viewport.height);
        hashCombine(l_Hash, l_Viewport.minDepth);
        hashCombine(l_Hash, l_Viewport.maxDepth);
    }
    hashCombine(l_Hash, std::hash<uint32_t>()(m_ViewportState.scissorCount));
    for (uint32_t i = 0; i < m_ViewportState.scissorCount; i++)
    {
        const VkRect2D& l_Scissor = m_ViewportState.pScissors[i];
        hashCombine(l_Hash, l_Scissor.offset.x);
        hashCombine(l_Hash, l_Scissor.offset.y);
        hashCombine(l_Hash, l_Scissor.extent.width);
        hashCombine(l_Hash, l_Scissor.extent.height);
    }
    return l_Hash;
}

size_t VulkanPipelineBuilder::getRasterizationHash() const
{
    size_t l_Hash;
    const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_RasterizationState.pNext);
    while (l_PNext)
    {
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT:
            {
                const VkDepthBiasRepresentationInfoEXT* l_DepthBiasInfo = reinterpret_cast<const VkDepthBiasRepresentationInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_DepthBiasInfo->depthBiasRepresentation);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineRasterizationConservativeStateCreateInfoEXT* l_ConservativeInfo = reinterpret_cast<const VkPipelineRasterizationConservativeStateCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_ConservativeInfo->conservativeRasterizationMode);
                hashCombine(l_Hash, l_ConservativeInfo->extraPrimitiveOverestimationSize);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineRasterizationDepthClipStateCreateInfoEXT* l_DepthClipInfo = reinterpret_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_DepthClipInfo->flags);
                hashCombine(l_Hash, l_DepthClipInfo->depthClipEnable);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO:
            {
                const VkPipelineRasterizationLineStateCreateInfo* l_LineInfo = reinterpret_cast<const VkPipelineRasterizationLineStateCreateInfo*>(l_PNext);
                hashCombine(l_Hash, l_LineInfo->lineRasterizationMode);
                hashCombine(l_Hash, l_LineInfo->stippledLineEnable);
                hashCombine(l_Hash, l_LineInfo->lineStippleFactor);
                hashCombine(l_Hash, l_LineInfo->lineStipplePattern);
                break;
        }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT* l_ProvokingVertexInfo = reinterpret_cast<const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_ProvokingVertexInfo->provokingVertexMode);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
            {
                const VkPipelineRasterizationStateRasterizationOrderAMD* l_RasterizationOrderInfo = reinterpret_cast<const VkPipelineRasterizationStateRasterizationOrderAMD*>(l_PNext);
                hashCombine(l_Hash, l_RasterizationOrderInfo->rasterizationOrder);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
            {
                const VkPipelineRasterizationStateStreamCreateInfoEXT* l_StreamInfo = reinterpret_cast<const VkPipelineRasterizationStateStreamCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_StreamInfo->flags);
                hashCombine(l_Hash, l_StreamInfo->rasterizationStream);
                break;
            }
        default:
            break;
        }
        l_PNext = l_PNext->pNext;
    }
    hashCombine(l_Hash, m_RasterizationState.flags);
    hashCombine(l_Hash, m_RasterizationState.depthClampEnable);
    hashCombine(l_Hash, m_RasterizationState.rasterizerDiscardEnable);
    hashCombine(l_Hash, m_RasterizationState.polygonMode);
    hashCombine(l_Hash, m_RasterizationState.cullMode);
    hashCombine(l_Hash, m_RasterizationState.frontFace);
    hashCombine(l_Hash, m_RasterizationState.depthBiasEnable);
    return l_Hash;
}

size_t VulkanPipelineBuilder::getMultisampleHash() const
{
    size_t l_Hash;
    const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_MultisampleState.pNext);
    while (l_PNext)
    {
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV:
            {
                const VkPipelineCoverageModulationStateCreateInfoNV* l_CoverageModulationInfo = reinterpret_cast<const VkPipelineCoverageModulationStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_CoverageModulationInfo->flags);
                hashCombine(l_Hash, l_CoverageModulationInfo->coverageModulationMode);
                hashCombine(l_Hash, l_CoverageModulationInfo->coverageModulationTableEnable);
                for (uint32_t i = 0; i < l_CoverageModulationInfo->coverageModulationTableCount; i++)
                {
                    hashCombine(l_Hash, l_CoverageModulationInfo->pCoverageModulationTable[i]);
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV:
            {
                const VkPipelineCoverageReductionStateCreateInfoNV* l_CoverageReductionInfo = reinterpret_cast<const VkPipelineCoverageReductionStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_CoverageReductionInfo->flags);
                hashCombine(l_Hash, l_CoverageReductionInfo->coverageReductionMode);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV:
            {
                const VkPipelineCoverageToColorStateCreateInfoNV* l_CoverageToColorInfo = reinterpret_cast<const VkPipelineCoverageToColorStateCreateInfoNV*>(l_PNext);
                hashCombine(l_Hash, l_CoverageToColorInfo->flags);
                hashCombine(l_Hash, l_CoverageToColorInfo->coverageToColorEnable);
                hashCombine(l_Hash, l_CoverageToColorInfo->coverageToColorLocation);
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineSampleLocationsStateCreateInfoEXT* l_SampleLocationsInfo = reinterpret_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_SampleLocationsInfo->sampleLocationsEnable);
                if (l_SampleLocationsInfo->sampleLocationsEnable)
                {
                    const VkSampleLocationsInfoEXT& l_SampleLocations = l_SampleLocationsInfo->sampleLocationsInfo;
                    hashCombine(l_Hash, l_SampleLocations.sampleLocationsPerPixel);
                    hashCombine(l_Hash, l_SampleLocations.sampleLocationGridSize.width);
                    hashCombine(l_Hash, l_SampleLocations.sampleLocationGridSize.height);
                    for (uint32_t i = 0; i < l_SampleLocations.sampleLocationsCount; i++)
                    {
                        const VkSampleLocationEXT& l_Location = l_SampleLocations.pSampleLocations[i];
                        hashCombine(l_Hash, l_Location.x);
                        hashCombine(l_Hash, l_Location.y);
                    }
                }
                break;
            }
        default:
            break;
        }
        l_PNext = l_PNext->pNext;
    }
    hashCombine(l_Hash, m_MultisampleState.flags);
    hashCombine(l_Hash, m_MultisampleState.rasterizationSamples);
    hashCombine(l_Hash, m_MultisampleState.sampleShadingEnable);
    hashCombine(l_Hash, m_MultisampleState.minSampleShading);
    hashCombine(l_Hash, m_MultisampleState.alphaToCoverageEnable);
    hashCombine(l_Hash, m_MultisampleState.alphaToOneEnable);
    // TODO: Sample mask
    return l_Hash;
}

size_t VulkanPipelineBuilder::getDepthStencilHash() const
{
    size_t l_Hash;
    hashCombine(l_Hash, m_DepthStencilState.flags);
    hashCombine(l_Hash, m_DepthStencilState.depthTestEnable);
    hashCombine(l_Hash, m_DepthStencilState.depthWriteEnable);
    hashCombine(l_Hash, m_DepthStencilState.depthCompareOp);
    hashCombine(l_Hash, m_DepthStencilState.depthBoundsTestEnable);
    hashCombine(l_Hash, m_DepthStencilState.stencilTestEnable);
    hashCombine(l_Hash, m_DepthStencilState.front.failOp);
    hashCombine(l_Hash, m_DepthStencilState.front.passOp);
    hashCombine(l_Hash, m_DepthStencilState.front.depthFailOp);
    hashCombine(l_Hash, m_DepthStencilState.front.compareOp);
    hashCombine(l_Hash, m_DepthStencilState.front.compareMask);
    hashCombine(l_Hash, m_DepthStencilState.front.writeMask);
    hashCombine(l_Hash, m_DepthStencilState.front.reference);
    hashCombine(l_Hash, m_DepthStencilState.back.failOp);
    hashCombine(l_Hash, m_DepthStencilState.back.passOp);
    hashCombine(l_Hash, m_DepthStencilState.back.depthFailOp);
    hashCombine(l_Hash, m_DepthStencilState.back.compareOp);
    hashCombine(l_Hash, m_DepthStencilState.back.compareMask);
    hashCombine(l_Hash, m_DepthStencilState.back.writeMask);
    hashCombine(l_Hash, m_DepthStencilState.back.reference);
    hashCombine(l_Hash, m_DepthStencilState.minDepthBounds);
    hashCombine(l_Hash, m_DepthStencilState.maxDepthBounds);
    return l_Hash;
}

size_t VulkanPipelineBuilder::getColorBlendHash() const
{
    size_t l_Hash;
    const VkBaseInStructure* l_PNext = static_cast<const VkBaseInStructure*>(m_ColorBlendState.pNext);
    while (l_PNext)
    {
        switch (l_PNext->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
            {
                const VkPipelineColorWriteCreateInfoEXT* l_ColorWriteInfo = reinterpret_cast<const VkPipelineColorWriteCreateInfoEXT*>(l_PNext);
                for (uint32_t i = 0; i < l_ColorWriteInfo->attachmentCount; i++)
                {
                    hashCombine(l_Hash, l_ColorWriteInfo->pColorWriteEnables[i]);
                }
                break;
            }
        case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
            {
                const VkPipelineColorBlendAdvancedStateCreateInfoEXT* l_AdvancedInfo = reinterpret_cast<const VkPipelineColorBlendAdvancedStateCreateInfoEXT*>(l_PNext);
                hashCombine(l_Hash, l_AdvancedInfo->srcPremultiplied);
                hashCombine(l_Hash, l_AdvancedInfo->dstPremultiplied);
                hashCombine(l_Hash, l_AdvancedInfo->blendOverlap);
                break;
            }
        default:
            break;
        }
        l_PNext = l_PNext->pNext;
    }
    hashCombine(l_Hash, m_ColorBlendState.flags);
    hashCombine(l_Hash, m_ColorBlendState.logicOpEnable);
    hashCombine(l_Hash, m_ColorBlendState.logicOp);
    for (uint32_t i = 0; i < m_ColorBlendState.attachmentCount; i++)
    {
        const VkPipelineColorBlendAttachmentState& l_Attachment = m_ColorBlendState.pAttachments[i];
        hashCombine(l_Hash, l_Attachment.blendEnable);
        hashCombine(l_Hash, l_Attachment.srcColorBlendFactor);
        hashCombine(l_Hash, l_Attachment.dstColorBlendFactor);
        hashCombine(l_Hash, l_Attachment.colorBlendOp);
        hashCombine(l_Hash, l_Attachment.srcAlphaBlendFactor);
        hashCombine(l_Hash, l_Attachment.dstAlphaBlendFactor);
        hashCombine(l_Hash, l_Attachment.alphaBlendOp);
        hashCombine(l_Hash, l_Attachment.colorWriteMask);
    }
    hashCombine(l_Hash, m_ColorBlendState.blendConstants[0]);
    hashCombine(l_Hash, m_ColorBlendState.blendConstants[1]);
    hashCombine(l_Hash, m_ColorBlendState.blendConstants[2]);
    hashCombine(l_Hash, m_ColorBlendState.blendConstants[3]);
    return l_Hash;
}

size_t VulkanPipelineBuilder::getDynamicStateHash() const
{
    size_t l_Hash;
    hashCombine(l_Hash, m_DynamicState.flags);
    for (uint32_t i = 0; i < m_DynamicState.dynamicStateCount; i++)
    {
        hashCombine(l_Hash, m_DynamicState.pDynamicStates[i]);
    }
    return l_Hash;
}

void VulkanPipelineBuilder::addShaderStage(const ResourceID p_Shader, const std::string_view p_Entrypoint)
{
    m_ShaderStages.emplace_back(p_Shader, p_Entrypoint);
}

void VulkanPipelineBuilder::resetShaderStages()
{
    m_ShaderStages.clear();
}

void VulkanPipelineBuilder::setVertexInputState(const VkPipelineVertexInputStateCreateInfo& p_State)
{
    m_VertexInputState = p_State;
    m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_VertexInputBindings.clear();
    m_VertexInputAttributes.clear();
}

void VulkanPipelineBuilder::setInputAssemblyState(const VkPipelineInputAssemblyStateCreateInfo& p_State)
{
    m_InputAssemblyState = p_State;
    m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setTessellationState(const VkPipelineTessellationStateCreateInfo& p_State)
{
    m_TessellationState = p_State;
    m_TessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    m_TesellationStateEnabled = true;
}

void VulkanPipelineBuilder::setViewportState(const VkPipelineViewportStateCreateInfo& p_State)
{
    m_ViewportState = p_State;
    m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setRasterizationState(const VkPipelineRasterizationStateCreateInfo& p_State)
{
    m_RasterizationState = p_State;
    m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setMultisampleState(const VkPipelineMultisampleStateCreateInfo& p_State)
{
    m_MultisampleState = p_State;
    m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setDepthStencilState(const VkPipelineDepthStencilStateCreateInfo& p_State)
{
    m_DepthStencilState = p_State;
    m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

void VulkanPipelineBuilder::setColorBlendState(const VkPipelineColorBlendStateCreateInfo& p_State)
{
    m_ColorBlendState = p_State;
    m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_Attachments.clear();
}

void VulkanPipelineBuilder::setDynamicState(const VkPipelineDynamicStateCreateInfo& p_State)
{
    m_DynamicState = p_State;
    m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
}

void VulkanPipeline::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

        l_Device.getTable().vkDestroyPipeline(l_Device.m_VkHandle, m_VkHandle, nullptr);
        LOG_DEBUG("Freed pipeline (ID: ", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

void VulkanPipelineBuilder::createShaderStages(VkPipelineShaderStageCreateInfo p_Container[]) const
{
    for (size_t i = 0; i < m_ShaderStages.size(); i++)
    {
        VkPipelineShaderStageCreateInfo l_StageInfo{};
        const VulkanShaderModule& l_Shader = VulkanContext::getDevice(m_Device).getShaderModule(m_ShaderStages[i].shader);
        l_StageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        l_StageInfo.stage = l_Shader.m_Stage;
        l_StageInfo.module = l_Shader.m_VkHandle;
        l_StageInfo.pName = m_ShaderStages[i].entrypoint.c_str();
        p_Container[i] = l_StageInfo;
    }
}

ResourceID VulkanPipeline::getLayout() const
{
    return m_Layout;
}

ResourceID VulkanPipeline::getRenderPass() const
{
    return m_RenderPass;
}

ResourceID VulkanPipeline::getSubpass() const
{
    return m_Subpass;
}

VkPipeline VulkanPipeline::operator*() const
{
    return m_VkHandle;
}

VulkanPipeline::VulkanPipeline(const ResourceID p_Device, const VkPipeline p_Handle, const ResourceID p_Layout, const ResourceID p_RenderPass, const ResourceID p_Subpass)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_Handle), m_Layout(p_Layout), m_RenderPass(p_RenderPass), m_Subpass(p_Subpass) {}

VkPipelineLayout VulkanPipelineLayout::operator*() const
{
    return m_VkHandle;
}

void VulkanPipelineLayout::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

        l_Device.getTable().vkDestroyPipelineLayout(l_Device.m_VkHandle, m_VkHandle, nullptr);
        LOG_DEBUG("Freed pipeline layout (ID: ", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

VulkanPipelineLayout::VulkanPipelineLayout(const uint32_t p_Device, const VkPipelineLayout p_Handle, const size_t p_Hash)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_Handle), m_Hash(p_Hash) {}

void VulkanComputePipeline::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

        l_Device.getTable().vkDestroyPipeline(*l_Device, m_VkHandle, nullptr);
        LOG_DEBUG("Freed compute pipeline (ID: ", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

VulkanComputePipeline::VulkanComputePipeline(const ResourceID p_Device, const VkPipeline p_Handle)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_Handle) {}
