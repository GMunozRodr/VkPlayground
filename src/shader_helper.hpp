#pragma once
#include <slang/slang.h>
#include <stdexcept>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>


inline SlangStage getSlangStageFromVkStage(const VkShaderStageFlagBits p_Stage)
{
    switch (p_Stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT: return SLANG_STAGE_VERTEX;
    case VK_SHADER_STAGE_FRAGMENT_BIT: return SLANG_STAGE_FRAGMENT;
    case VK_SHADER_STAGE_COMPUTE_BIT: return SLANG_STAGE_COMPUTE;
    case VK_SHADER_STAGE_GEOMETRY_BIT: return SLANG_STAGE_GEOMETRY;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return SLANG_STAGE_HULL;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return SLANG_STAGE_DOMAIN;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR: return SLANG_STAGE_RAY_GENERATION;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: return SLANG_STAGE_ANY_HIT;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: return SLANG_STAGE_CLOSEST_HIT;
    case VK_SHADER_STAGE_MISS_BIT_KHR: return SLANG_STAGE_MISS;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: return SLANG_STAGE_INTERSECTION;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR: return SLANG_STAGE_CALLABLE;
    case VK_SHADER_STAGE_MESH_BIT_EXT: return SLANG_STAGE_MESH;
    default:
        throw std::runtime_error("Unsupported shader stage");
    }
}

inline VkShaderStageFlagBits getVkStageFromSlangStage(const SlangStage p_Stage)
{
    switch (p_Stage)
    {
    case SLANG_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
    case SLANG_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case SLANG_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
    case SLANG_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case SLANG_STAGE_HULL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case SLANG_STAGE_DOMAIN: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case SLANG_STAGE_RAY_GENERATION: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case SLANG_STAGE_ANY_HIT: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case SLANG_STAGE_CLOSEST_HIT: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case SLANG_STAGE_MISS: return VK_SHADER_STAGE_MISS_BIT_KHR;
    case SLANG_STAGE_INTERSECTION: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case SLANG_STAGE_CALLABLE: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    case SLANG_STAGE_MESH: return VK_SHADER_STAGE_MESH_BIT_EXT;
    default:
        throw std::runtime_error("Unsupported shader stage");
    }
}

template<typename T>
static void hashCombine(size_t& p_Seed, const T& p_Element)
{
    const size_t l_Hash = std::hash<T>()(p_Element);
    p_Seed ^= l_Hash + 0x9e3779b9 + (p_Seed << 6) + (p_Seed >> 2);
};