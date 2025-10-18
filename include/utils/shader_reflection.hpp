#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <slang/slang.h>
#include <Volk/volk.h>

#include "vulkan_base.hpp"
#include "vulkan_binding.hpp"
#include "vulkan_shader.hpp"

struct ShaderReflectionData
{
    ShaderReflectionData() = default;
    explicit ShaderReflectionData(slang::ProgramLayout* p_Layout);

    enum FieldType: uint8_t
    {
        BOOL = 0, BOOL2 = 2, BOOL3 = 3, BOOL4 = 4, BOOL2X2 = 5, BOOL3X3 = 10, BOOL4X4 = 17,
        INT8 = 144, INT8_2 = 146, INT8_3 = 147, INT8_4 = 148, INT8_2X2 = 149, INT8_3X3 = 154, INT8_4X4 = 161,
        INT16 = 180, INT16_2 = 182, INT16_3 = 183, INT16_4 = 184, INT16_2X2 = 185, INT16_3X3 = 190, INT16_4X4 = 197,
        INT32 = 18, INT32_2 = 20, INT32_3 = 21, INT32_4 = 22, INT32_2X2 = 23, INT32_3X3 = 28, INT32_4X4 = 35,
        INT64 = 54, INT64_2 = 56, INT64_3 = 57, INT64_4 = 58, INT64_2X2 = 59, INT64_3X3 = 64, INT64_4X4 = 71,
        UINT8 = 162, UINT8_2 = 164, UINT8_3 = 165, UINT8_4 = 166, UINT8_2X2 = 167, UINT8_3X3 = 172, UINT8_4X4 = 179,
        UINT16 = 198, UINT16_2 = 200, UINT16_3 = 201, UINT16_4 = 202, UINT16_2X2 = 203, UINT16_3X3 = 208, UINT16_4X4 = 215,
        UINT32 = 36, UINT32_2 = 38, UINT32_3 = 39, UINT32_4 = 40, UINT32_2X2 = 41, UINT32_3X3 = 46, UINT32_4X4 = 53,
        UINT64 = 72, UINT64_2 = 74, UINT64_3 = 75, UINT64_4 = 76, UINT64_2X2 = 77, UINT64_3X3 = 82, UINT64_4X4 = 89,
        FLOAT16 = 90, FLOAT16_2 = 92, FLOAT16_3 = 93, FLOAT16_4 = 94, FLOAT16_2X2 = 95, FLOAT16_3X3 = 100, FLOAT16_4X4 = 107,
        FLOAT32 = 108, FLOAT32_2 = 110, FLOAT32_3 = 111, FLOAT32_4 = 112, FLOAT32_2X2 = 113, FLOAT32_3X3 = 118, FLOAT32_4X4 = 125,
        FLOAT64 = 126, FLOAT64_2 = 128, FLOAT64_3 = 129, FLOAT64_4 = 130, FLOAT64_2X2 = 131, FLOAT64_3X3 = 136, FLOAT64_4X4 = 143,

        IMAGE1D = 244, IMAGE2D = 245, IMAGE3D = 246, BUFFER = 247, SAMPLER = 248, SUBPASS_INPUT = 249,

        UNKNOWN = UINT8_MAX
    };

    struct TypeData
    {
        size_t numElements;
        FieldType type;
    };

    struct Field
    {
        virtual ~Field() = default;

        std::string name;
        size_t size;
    };

    using FieldPtr = std::shared_ptr<Field>;

    struct Variable final : Field
    {
        TypeData data;
        uint32_t binding;
    };

    using VariablePtr = std::shared_ptr<Variable>;

    struct Struct final : Field
    {
        std::vector<FieldPtr> members;
    };

    using StructPtr = std::shared_ptr<Struct>;

    struct Resource final : Field
    {
        FieldType type;
        FieldType subType;
        bool readOnly;
    };

    using ResourcePtr = std::shared_ptr<Resource>;

    struct VertexBindingRef
    {
        struct Field
        {
            VariablePtr field;
            uint32_t topLevelIndex;
        };

        VulkanBinding& binding;
        uint32_t fields;
    };

    struct DescriptorBinding
    {
        uint32_t binding;
        uint32_t set;
        FieldPtr field;
    };

    std::vector<FieldPtr> vertexInputs;
    std::vector<FieldPtr> fragmentOutputs;
    std::vector<DescriptorBinding> descriptorBindings;
    StructPtr pushConstantBlock;

    VkShaderStageFlags stageFlags = 0;

    [[nodiscard]] VkPushConstantRange getPushConstantRange() const;
    void populateBindings(std::span<VertexBindingRef> p_Bindings, uint32_t p_StartingField = 0) const;

    void invalidate() { m_Valid = false; }
    [[nodiscard]] bool isValid() const { return m_Valid; }

private:
    struct FieldData
    {
        enum Category : uint8_t { INPUT, PUSH, DESCRIPTOR, INVALID };

        Category category;
        FieldPtr field;
    };

    struct BindingFormat
    {
        VkFormat format;
        uint32_t bindingSize;
    };

    bool m_Valid = false;

    static FieldData createField(slang::VariableLayoutReflection* p_Variable, uint32_t p_BindingOffset);
    static TypeData getTypeData(slang::TypeLayoutReflection* p_Type);
    static FieldType getTypeFromShape(SlangResourceShape p_Shape);
    static FieldType getType(slang::TypeReflection* p_Type);
    static BindingFormat getBindingFormat(FieldType p_Type);

    [[nodiscard]] std::vector<VertexBindingRef::Field> getFlatVertexInputs(uint32_t p_StartingPoint = 0) const;
    static void fillFlatVertexInputs(std::vector<VertexBindingRef::Field>& p_Vector, const FieldPtr& p_Input, uint32_t p_TopLevelIndex);

    static constexpr std::array<FieldType, 14> s_ScalarKindMapping = {UNKNOWN, UNKNOWN, BOOL, INT32, UINT32, INT64, UINT64, FLOAT16, FLOAT32, FLOAT64, INT8, UINT8, INT16, UINT16};
};

inline ShaderReflectionData::ShaderReflectionData(slang::ProgramLayout* p_Layout)
    : m_Valid(true)
{
    for (uint32_t i = 0; i < p_Layout->getEntryPointCount(); i++)
    {
        slang::EntryPointLayout* l_EntryPoint = p_Layout->getEntryPointByIndex(i);
        const VkShaderStageFlagBits l_Stage = getVkStageFromSlangStage(l_EntryPoint->getStage());
        stageFlags |= l_Stage;
        if (l_Stage == VK_SHADER_STAGE_VERTEX_BIT)
        {
            for (uint32_t j = 0; j < l_EntryPoint->getParameterCount(); j++)
            {
                slang::VariableLayoutReflection* l_Variable = l_EntryPoint->getParameterByIndex(j);
                FieldData l_Field = createField(l_Variable, 0);
                if (l_Field.category == FieldData::INPUT)
                {
                    vertexInputs.push_back(l_Field.field);
                }
                else
                {
                    LOG_WARN("Vertex parameter is not an input variable");
                }
            }
        }
        else if (l_Stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            slang::VariableLayoutReflection* l_Result = l_EntryPoint->getResultVarLayout();
            if (l_Result->getTypeLayout()->getKind() == slang::TypeReflection::Kind::Scalar)
            {
                FieldData l_Field = createField(l_Result, 0);
                fragmentOutputs.push_back(l_Field.field);
            }
            else if (l_Result->getTypeLayout()->getKind() == slang::TypeReflection::Kind::Struct)
            {
                for (uint32_t j = 0; j < l_Result->getTypeLayout()->getFieldCount(); j++)
                {
                    FieldData l_Field = createField(l_Result->getTypeLayout()->getFieldByIndex(j), 0);
                    fragmentOutputs.push_back(l_Field.field);
                }
            }
        }
    }

    for (uint32_t i = 0; i < p_Layout->getParameterCount(); i++)
    {
        slang::VariableLayoutReflection* l_Variable = p_Layout->getParameterByIndex(i);
        FieldData l_Field = createField(l_Variable, 0);
        if (l_Field.category == FieldData::PUSH)
        {
            if (pushConstantBlock)
            {
                LOG_WARN("Multiple push constant blocks found in shader");
            }
            if (const StructPtr l_Struct = std::dynamic_pointer_cast<Struct>(l_Field.field))
            {
                pushConstantBlock = l_Struct;
            }
            else
            {
                LOG_WARN("Push constant block is not a struct");
            }
        }
        else if (l_Field.category == FieldData::DESCRIPTOR)
        {
            DescriptorBinding l_DescriptorBinding;
            l_DescriptorBinding.binding = l_Variable->getBindingIndex();
            l_DescriptorBinding.set = l_Variable->getBindingSpace();
            l_DescriptorBinding.field = l_Field.field;
            descriptorBindings.push_back(l_DescriptorBinding);
        }
        else
        {
            LOG_WARN("Shader parameter is not a descriptor variable nor push constant");
        }
    }
}

inline VkPushConstantRange ShaderReflectionData::getPushConstantRange() const
{
    VkPushConstantRange l_PushConstantRange{};
    l_PushConstantRange.size = static_cast<uint32_t>(pushConstantBlock->size);
    l_PushConstantRange.stageFlags = stageFlags;
    return l_PushConstantRange;
}

inline void ShaderReflectionData::populateBindings(std::span<VertexBindingRef> p_Bindings, const uint32_t p_StartingField) const
{
    const std::vector<VertexBindingRef::Field> l_FlatVertexInputs = getFlatVertexInputs(p_StartingField);
    for (uint32_t i = 0; i < p_Bindings.size(); i++)
    {
        std::vector<VertexBindingRef::Field> l_BindingFields;

        const uint32_t l_MaxFieldIndex = i + p_Bindings[i].fields - 1;
        for (const VertexBindingRef::Field& l_Field : l_FlatVertexInputs)
        {
            if (l_Field.topLevelIndex >= i && l_Field.topLevelIndex <= l_MaxFieldIndex)
            {
                l_BindingFields.push_back(l_Field);
            }
        }

        std::ranges::sort(l_BindingFields, [](const VertexBindingRef::Field& p_A, const VertexBindingRef::Field& p_B) { return p_A.field->binding < p_B.field->binding; });

        uint32_t l_Offset = 0;
        for (const VertexBindingRef::Field& l_Field : l_BindingFields)
        {
            if (l_Field.field->binding == UINT32_MAX)
            {
                continue;
            }
            VertexBindingRef& l_Binding = p_Bindings[i];
            const BindingFormat l_BindingFormat = getBindingFormat(l_Field.field->data.type);
            l_Binding.binding.addAttribDescription(l_BindingFormat.format, l_Offset, l_Field.field->binding, l_BindingFormat.bindingSize);
            l_Offset += static_cast<uint32_t>(l_Field.field->size);
        }
    }
}

inline ShaderReflectionData::FieldData ShaderReflectionData::createField(slang::VariableLayoutReflection* p_Variable, const uint32_t p_BindingOffset)
{
    slang::TypeLayoutReflection* l_Type = p_Variable->getTypeLayout();
    const slang::ParameterCategory l_SlangCategory = p_Variable->getCategory();
    FieldData::Category l_Category;
    switch (l_SlangCategory)
    {
    case slang::ParameterCategory::VaryingInput:
        l_Category = FieldData::INPUT;
        break;
    case slang::ParameterCategory::PushConstantBuffer:
        l_Category = FieldData::PUSH;
        break;
    case slang::ParameterCategory::DescriptorTableSlot:
        l_Category = FieldData::DESCRIPTOR;
        break;
    default:
        l_Category = FieldData::INVALID;
        break;
    }

    FieldPtr l_Field;
    std::string l_Name;
    if (p_Variable->getName() != nullptr)
    {
        l_Name = p_Variable->getName();
    }
    const slang::TypeReflection::Kind l_Kind = l_Type->getKind();
    if (l_Kind == slang::TypeReflection::Kind::Struct)
    {
        const StructPtr l_Struct = std::make_shared<Struct>();
        for (uint32_t i = 0; i < l_Type->getFieldCount(); i++)
        {
            FieldData l_FieldData = createField(l_Type->getFieldByIndex(i), p_BindingOffset + p_Variable->getBindingIndex());
            l_Struct->members.push_back(l_FieldData.field);
        }

        l_Field = l_Struct;
    }
    else if (l_Kind == slang::TypeReflection::Kind::ConstantBuffer)
    {
        const FieldData l_StructData = createField(l_Type->getElementVarLayout(), 0);
        l_Field = l_StructData.field;
    }
    else if (l_Kind == slang::TypeReflection::Kind::Resource)
    {
        const ResourcePtr l_Resource = std::make_shared<Resource>();

        l_Resource->type = getType(l_Type->getType());
        if (l_Resource->type == UNKNOWN)
        {
            l_Resource->type = getTypeFromShape(l_Type->getResourceShape());
            if (l_Resource->type == BUFFER)
            {
                l_Resource->subType = getType(l_Type->getElementTypeLayout()->getType());
            }
            else
            {
                l_Resource->subType = getType(l_Type->getResourceResultType());
            }
        }

        const SlangResourceAccess l_Access = l_Type->getResourceAccess();
        if (l_Access != SLANG_RESOURCE_ACCESS_READ && l_Access != SLANG_RESOURCE_ACCESS_READ_WRITE)
        {
            LOG_WARN("Resource access is not read or read/write");
        }
        l_Resource->readOnly = l_Access == SLANG_RESOURCE_ACCESS_READ;

        l_Field = l_Resource;
    }
    else
    {
        const VariablePtr l_Variable = std::make_shared<Variable>();
        l_Variable->data = getTypeData(l_Type);
        l_Variable->binding = p_Variable->getBindingIndex() + p_BindingOffset;

        l_Field = l_Variable;
    }

    l_Field->name = l_Name;
    l_Field->size = l_Type->getSize();
    return {.category = l_Category, .field = std::shared_ptr<Field>(l_Field)};
}

inline ShaderReflectionData::TypeData ShaderReflectionData::getTypeData(slang::TypeLayoutReflection* p_Type)
{
    slang::TypeLayoutReflection* l_Type = p_Type;

    const TypeData l_TypeData{.numElements = std::max(1ULL, l_Type->getTotalArrayElementCount()), .type = getType(l_Type->unwrapArray()->getType()),};

    return l_TypeData;
}

inline ShaderReflectionData::FieldType ShaderReflectionData::getTypeFromShape(const SlangResourceShape p_Shape)
{
    switch (p_Shape)
    {
    case SLANG_TEXTURE_1D:
        return IMAGE1D;
    case SLANG_TEXTURE_2D:
        return IMAGE2D;
    case SLANG_TEXTURE_3D:
        return IMAGE3D;
    case SLANG_STRUCTURED_BUFFER:
        return BUFFER;
    case SLANG_TEXTURE_SUBPASS:
        return SUBPASS_INPUT;
    default:
        return UNKNOWN;
    }
}

inline ShaderReflectionData::FieldType ShaderReflectionData::getType(slang::TypeReflection* p_Type)
{
    switch (p_Type->getKind())
    {
    case slang::TypeReflection::Kind::SamplerState:
        return SAMPLER;
    case slang::TypeReflection::Kind::Scalar:
        return s_ScalarKindMapping[p_Type->getScalarType()];
    case slang::TypeReflection::Kind::Vector:
        return static_cast<FieldType>(getType(p_Type->getElementType()) + p_Type->getElementCount());
    case slang::TypeReflection::Kind::Matrix:
        return static_cast<FieldType>(getType(p_Type->getElementType()->getElementType()) + p_Type->getRowCount() * p_Type->getColumnCount() + 1);
    default:
        return UNKNOWN;
    }
}

inline std::vector<ShaderReflectionData::VertexBindingRef::Field> ShaderReflectionData::getFlatVertexInputs(const uint32_t p_StartingPoint) const
{
    using BindingField = VertexBindingRef::Field;

    std::vector<BindingField> l_FlatVertexInputs;
    for (uint32_t i = p_StartingPoint; i < vertexInputs.size(); i++)
    {
        fillFlatVertexInputs(l_FlatVertexInputs, vertexInputs[i], i);
    }
    return l_FlatVertexInputs;
}

inline void ShaderReflectionData::fillFlatVertexInputs(std::vector<VertexBindingRef::Field>& p_Vector, const FieldPtr& p_Input, const uint32_t p_TopLevelIndex)
{
    if (const VariablePtr l_Var = std::dynamic_pointer_cast<Variable>(p_Input))
    {
        p_Vector.push_back({.field = l_Var, .topLevelIndex = p_TopLevelIndex});
    }
    else if (const StructPtr l_Struct = std::dynamic_pointer_cast<Struct>(p_Input))
    {
        for (const FieldPtr& l_Member : l_Struct->members)
        {
            fillFlatVertexInputs(p_Vector, l_Member, p_TopLevelIndex);
        }
    }
}

inline ShaderReflectionData::BindingFormat ShaderReflectionData::getBindingFormat(const FieldType p_Type)
{
    switch (p_Type)
    {
    case BOOL: return {.format = VK_FORMAT_R8_UINT, .bindingSize = 1};
    case BOOL2: return {.format = VK_FORMAT_R8G8_UINT, .bindingSize = 1};
    case BOOL3: return {.format = VK_FORMAT_R8G8B8_UINT, .bindingSize = 1};
    case BOOL4: return {.format = VK_FORMAT_R8G8B8A8_UINT, .bindingSize = 1};
    case BOOL2X2: return {.format = VK_FORMAT_R8G8_UINT, .bindingSize = 2};
    case BOOL3X3: return {.format = VK_FORMAT_R8G8B8_UINT, .bindingSize = 3};
    case BOOL4X4: return {.format = VK_FORMAT_R8G8B8A8_UINT, .bindingSize = 4};
    case INT8: return {.format = VK_FORMAT_R8_SINT, .bindingSize = 1};
    case INT8_2: return {.format = VK_FORMAT_R8G8_SINT, .bindingSize = 1};
    case INT8_3: return {.format = VK_FORMAT_R8G8B8_SINT, .bindingSize = 1};
    case INT8_4: return {.format = VK_FORMAT_R8G8B8A8_SINT, .bindingSize = 1};
    case INT8_2X2: return {.format = VK_FORMAT_R8G8_SINT, .bindingSize = 2};
    case INT8_3X3: return {.format = VK_FORMAT_R8G8B8_SINT, .bindingSize = 3};
    case INT8_4X4: return {.format = VK_FORMAT_R8G8B8A8_SINT, .bindingSize = 4};
    case INT16: return {.format = VK_FORMAT_R16_SINT, .bindingSize = 1};
    case INT16_2: return {.format = VK_FORMAT_R16G16_SINT, .bindingSize = 1};
    case INT16_3: return {.format = VK_FORMAT_R16G16B16_SINT, .bindingSize = 1};
    case INT16_4: return {.format = VK_FORMAT_R16G16B16A16_SINT, .bindingSize = 1};
    case INT16_2X2: return {.format = VK_FORMAT_R16G16_SINT, .bindingSize = 2};
    case INT16_3X3: return {.format = VK_FORMAT_R16G16B16_SINT, .bindingSize = 3};
    case INT16_4X4: return {.format = VK_FORMAT_R16G16B16A16_SINT, .bindingSize = 4};
    case INT32: return {.format = VK_FORMAT_R32_SINT, .bindingSize = 1};
    case INT32_2: return {.format = VK_FORMAT_R32G32_SINT, .bindingSize = 1};
    case INT32_3: return {.format = VK_FORMAT_R32G32B32_SINT, .bindingSize = 1};
    case INT32_4: return {.format = VK_FORMAT_R32G32B32A32_SINT, .bindingSize = 1};
    case INT32_2X2: return {.format = VK_FORMAT_R32G32_SINT, .bindingSize = 2};
    case INT32_3X3: return {.format = VK_FORMAT_R32G32B32_SINT, .bindingSize = 3};
    case INT32_4X4: return {.format = VK_FORMAT_R32G32B32A32_SINT, .bindingSize = 4};
    case INT64: return {.format = VK_FORMAT_R64_SINT, .bindingSize = 1};
    case INT64_2: return {.format = VK_FORMAT_R64G64_SINT, .bindingSize = 1};
    case INT64_3: return {.format = VK_FORMAT_R64G64B64_SINT, .bindingSize = 1};
    case INT64_4: return {.format = VK_FORMAT_R64G64B64A64_SINT, .bindingSize = 1};
    case INT64_2X2: return {.format = VK_FORMAT_R64G64_SINT, .bindingSize = 2};
    case INT64_3X3: return {.format = VK_FORMAT_R64G64B64_SINT, .bindingSize = 3};
    case INT64_4X4: return {.format = VK_FORMAT_R64G64B64A64_SINT, .bindingSize = 4};
    case UINT8: return {.format = VK_FORMAT_R8_UINT, .bindingSize = 1};
    case UINT8_2: return {.format = VK_FORMAT_R8G8_UINT, .bindingSize = 1};
    case UINT8_3: return {.format = VK_FORMAT_R8G8B8_UINT, .bindingSize = 1};
    case UINT8_4: return {.format = VK_FORMAT_R8G8B8A8_UINT, .bindingSize = 1};
    case UINT8_2X2: return {.format = VK_FORMAT_R8G8_UINT, .bindingSize = 2};
    case UINT8_3X3: return {.format = VK_FORMAT_R8G8B8_UINT, .bindingSize = 3};
    case UINT8_4X4: return {.format = VK_FORMAT_R8G8B8A8_UINT, .bindingSize = 4};
    case UINT16: return {.format = VK_FORMAT_R16_UINT, .bindingSize = 1};
    case UINT16_2: return {.format = VK_FORMAT_R16G16_UINT, .bindingSize = 1};
    case UINT16_3: return {.format = VK_FORMAT_R16G16B16_UINT, .bindingSize = 1};
    case UINT16_4: return {.format = VK_FORMAT_R16G16B16A16_UINT, .bindingSize = 1};
    case UINT16_2X2: return {.format = VK_FORMAT_R16G16_UINT, .bindingSize = 2};
    case UINT16_3X3: return {.format = VK_FORMAT_R16G16B16_UINT, .bindingSize = 3};
    case UINT16_4X4: return {.format = VK_FORMAT_R16G16B16A16_UINT, .bindingSize = 4};
    case UINT32: return {.format = VK_FORMAT_R32_UINT, .bindingSize = 1};
    case UINT32_2: return {.format = VK_FORMAT_R32G32_UINT, .bindingSize = 1};
    case UINT32_3: return {.format = VK_FORMAT_R32G32B32_UINT, .bindingSize = 1};
    case UINT32_4: return {.format = VK_FORMAT_R32G32B32A32_UINT, .bindingSize = 1};
    case UINT32_2X2: return {.format = VK_FORMAT_R32G32_UINT, .bindingSize = 2};
    case UINT32_3X3: return {.format = VK_FORMAT_R32G32B32_UINT, .bindingSize = 3};
    case UINT32_4X4: return {.format = VK_FORMAT_R32G32B32A32_UINT, .bindingSize = 4};
    case UINT64: return {.format = VK_FORMAT_R64_UINT, .bindingSize = 1};
    case UINT64_2: return {.format = VK_FORMAT_R64G64_UINT, .bindingSize = 1};
    case UINT64_3: return {.format = VK_FORMAT_R64G64B64_UINT, .bindingSize = 1};
    case UINT64_4: return {.format = VK_FORMAT_R64G64B64A64_UINT, .bindingSize = 1};
    case UINT64_2X2: return {.format = VK_FORMAT_R64G64_UINT, .bindingSize = 2};
    case UINT64_3X3: return {.format = VK_FORMAT_R64G64B64_UINT, .bindingSize = 3};
    case UINT64_4X4: return {.format = VK_FORMAT_R64G64B64A64_UINT, .bindingSize = 4};
    case FLOAT16: return {.format = VK_FORMAT_R16_SFLOAT, .bindingSize = 1};
    case FLOAT16_2: return {.format = VK_FORMAT_R16G16_SFLOAT, .bindingSize = 1};
    case FLOAT16_3: return {.format = VK_FORMAT_R16G16B16_SFLOAT, .bindingSize = 1};
    case FLOAT16_4: return {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .bindingSize = 1};
    case FLOAT16_2X2: return {.format = VK_FORMAT_R16G16_SFLOAT, .bindingSize = 2};
    case FLOAT16_3X3: return {.format = VK_FORMAT_R16G16B16_SFLOAT, .bindingSize = 3};
    case FLOAT16_4X4: return {.format = VK_FORMAT_R16G16B16A16_SFLOAT, .bindingSize = 4};
    case FLOAT32: return {.format = VK_FORMAT_R32_SFLOAT, .bindingSize = 1};
    case FLOAT32_2: return {.format = VK_FORMAT_R32G32_SFLOAT, .bindingSize = 1};
    case FLOAT32_3: return {.format = VK_FORMAT_R32G32B32_SFLOAT, .bindingSize = 1};
    case FLOAT32_4: return {.format = VK_FORMAT_R32G32B32A32_SFLOAT, .bindingSize = 1};
    case FLOAT32_2X2: return {.format = VK_FORMAT_R32G32_SFLOAT, .bindingSize = 2};
    case FLOAT32_3X3: return {.format = VK_FORMAT_R32G32B32_SFLOAT, .bindingSize = 3};
    case FLOAT32_4X4: return {.format = VK_FORMAT_R32G32B32A32_SFLOAT, .bindingSize = 4};
    case FLOAT64: return {.format = VK_FORMAT_R64_SFLOAT, .bindingSize = 1};
    case FLOAT64_2: return {.format = VK_FORMAT_R64G64_SFLOAT, .bindingSize = 1};
    case FLOAT64_3: return {.format = VK_FORMAT_R64G64B64_SFLOAT, .bindingSize = 1};
    case FLOAT64_4: return {.format = VK_FORMAT_R64G64B64A64_SFLOAT, .bindingSize = 1};
    case FLOAT64_2X2: return {.format = VK_FORMAT_R64G64_SFLOAT, .bindingSize = 2};
    case FLOAT64_3X3: return {.format = VK_FORMAT_R64G64B64_SFLOAT, .bindingSize = 3};
    case FLOAT64_4X4: return {.format = VK_FORMAT_R64G64B64A64_SFLOAT, .bindingSize = 4};
    case UNKNOWN: default:
        return {.format = VK_FORMAT_UNDEFINED, .bindingSize = 0};
    }
}
