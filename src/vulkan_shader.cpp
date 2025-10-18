#include "vulkan_shader.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#include "shader_helper.hpp"
#include "vulkan_context.hpp"
#include "vulkan_device.hpp"
#include "utils/call_on_destroy.hpp"
#include "utils/logger.hpp"

static void printBlob(slang::IBlob* p_Blob)
{
    if (p_Blob)
    {
        LOG_ERR(static_cast<const char*>(p_Blob->getBufferPointer()));
        p_Blob->release();
    }
}

void ShaderHash::addFile(const std::filesystem::path& p_File)
{
    std::ifstream l_File(p_File, std::ios::binary);
    if (!l_File.is_open())
    {
        throw std::runtime_error("Failed to open shader file for hashing: " + p_File.string());
    }

    std::stringstream l_Stream;
    l_Stream << l_File.rdbuf();
    l_File.close();

    m_Data.push_back(l_Stream.str());
}

void ShaderHash::addFolder(const std::filesystem::path& p_Folder, bool p_Recursive)
{
    if (!std::filesystem::exists(p_Folder) || !std::filesystem::is_directory(p_Folder))
    {
        throw std::runtime_error("Failed to open folder for hashing: " + p_Folder.string());
    }

    if (p_Recursive)
    {
        for (const auto& l_Entry : std::filesystem::recursive_directory_iterator(p_Folder))
        {
            if (l_Entry.is_regular_file())
            {
                const std::string l_Extension = l_Entry.path().extension().string();
                if (l_Extension == ".slang" || l_Extension == ".slangh")
                {
                    addFile(l_Entry.path());
                }
            }
        }
    }
    else
    {
        for (const auto& l_Entry : std::filesystem::directory_iterator(p_Folder))
        {
            if (l_Entry.is_regular_file())
            {
                const std::string l_Extension = l_Entry.path().extension().string();
                if (l_Extension == ".slang" || l_Extension == ".slangh")
                {
                    addFile(l_Entry.path());
                }
            }
        }
    }
}

void ShaderHash::addString(const std::string& p_Content)
{
    m_Data.push_back(p_Content);
}

void ShaderHash::addMacro(const std::string& p_Name, const std::string& p_Value)
{
    m_Data.push_back(p_Name + "=" + p_Value);
}

size_t ShaderHash::getHash() const
{
    if (m_HashValue == 0)
    {
        size_t l_CombinedHash = 0;
        for (const std::string& l_FileContent : m_Data)
        {
            hashCombine(l_CombinedHash, l_FileContent);
        }

        m_HashValue = l_CombinedHash;
    }
    return m_HashValue;
}

void VulkanShader::reset(VulkanShader& p_Shader)
{
    p_Shader.~VulkanShader();
    new (&p_Shader) VulkanShader{};
}

void VulkanShader::reinit(VulkanShader& p_Shader, const ThreadID p_CompilationThread, const bool p_Optimize, const std::span<const MacroDef> p_Macros)
{
    p_Shader.~VulkanShader();
    new (&p_Shader) VulkanShader{p_CompilationThread, p_Optimize, p_Macros};
}

VulkanShader::VulkanShader(const ThreadID p_CompilationThread, const bool p_Optimize, const std::span<const MacroDef> p_Macros)
    : m_Macros(p_Macros.begin(), p_Macros.end()), m_Optimize(p_Optimize), m_CompilationThread(p_CompilationThread) {}

VulkanShader::~VulkanShader()
{
    if (m_SlangProgram)
    {
        m_SlangProgram->release();
        m_SlangProgram = nullptr;
    }

    if (m_SlangSession)
    {
        m_SlangSession->release();
        m_SlangSession = nullptr;
    }
}

VulkanShader::VulkanShader(VulkanShader&& p_Other) noexcept
{
    m_SlangSession = p_Other.m_SlangSession;
    p_Other.m_SlangSession = nullptr;
    m_SlangProgram = p_Other.m_SlangProgram;
    p_Other.m_SlangProgram = nullptr;
    m_SlangComponents = std::move(p_Other.m_SlangComponents);
    m_Result = p_Other.m_Result;
    m_Macros = std::move(p_Other.m_Macros);
}

void VulkanShader::enableCache(std::filesystem::path p_CacheFile)
{
    m_UseCache = true;
    m_HashFile = std::move(p_CacheFile);
}

void VulkanShader::setExpectedStages(const VkShaderStageFlags p_Stages, std::span<std::string> p_EntryPoints)
{
    m_ExpectedStages = p_Stages;
    m_ExpectedEntryPoints = std::vector<std::string>(p_EntryPoints.begin(), p_EntryPoints.end());
}

void VulkanShader::addModule(const std::string_view p_Filename, const std::string_view p_ModuleName)
{
    m_Modules.emplace_back(std::string(p_Filename), ModuleData::SourceType::FILE, std::string(p_ModuleName));
    if (m_UseCache)
    {
        m_Hash.addFile(m_Modules.back().data);
        m_Hash.addMacro("type", "file");
        m_Hash.addMacro("name", m_Modules.back().name);
    }
}

void VulkanShader::addModuleString(const std::string_view p_Source, const std::string_view p_ModuleName)
{
    m_Modules.emplace_back(std::string(p_Source), ModuleData::SourceType::STRING, std::string(p_ModuleName));
    if (m_UseCache)
    {
        m_Hash.addString(m_Modules.back().data);
        m_Hash.addMacro("type", "str");
        m_Hash.addMacro("name", m_Modules.back().name);
    }
}

void VulkanShader::compile(const bool p_ForceCompile)
{
    if (m_UseCache)
    {
        size_t l_HashValue = m_Hash.getHash();
        hashCombine(l_HashValue, static_cast<size_t>(m_Optimize));
        hashCombine(l_HashValue, std::string(SPIRV_PROFILE));
        if (tryLoadCachedCode(l_HashValue))
        {
            LOG_DEBUG("Loaded shader from cache:", m_HashFile.string());
            m_Result.status = Result::CACHED;
            if (!p_ForceCompile)
                return;
        }
        else
        {
            LOG_INFO("Shader cache file ", m_HashFile.string(), " is missing or not valid, compiling shader");
        }
        
    }

    buildSession(m_CompilationThread, m_Optimize);

    for (const ModuleData& l_Module : m_Modules)
    {
        if (l_Module.type == ModuleData::SourceType::FILE)
        {
            loadModule(l_Module.data, l_Module.name);
        }
        else
        {
            loadModuleString(l_Module.data, l_Module.name);
        }
    }
    linkAndFinalize();

    if (m_UseCache)
    {
        saveCodeCache();
    }
}

bool VulkanShader::tryLoadCachedCode(const size_t p_HashValue)
{
    // File structure:
    // [CacheHeader]
    //   - uint32_t magic
    //   - uint32_t version
    //   - char[32] slangVersion
    //   - char[16] spirvProfile
    //   - size_t contentHash
    // [uint32_t stage count]
    // For each stage:
    //   [uint32_t stage]
    //   [uint32_t name length]
    //   [name bytes]
    //   [uint32_t code size in bytes]
    //   [code bytes]

    std::ifstream l_File{ m_HashFile, std::ios::binary };
    if (!l_File.is_open())
    {
        return false;
    }

    CacheHeader l_Header;
    l_File.read(reinterpret_cast<char*>(&l_Header), sizeof(CacheHeader));

    if (l_Header.magic != CACHE_MAGIC)
    {
        LOG_INFO("Shader cache invalid magic number for file ", m_HashFile.string());
        return false;
    }

    if (l_Header.version != CACHE_VERSION)
    {
        LOG_INFO("Shader cache version mismatch for file ", m_HashFile.string());
        return false;
    }

    if (std::strcmp(l_Header.spirvProfile, SPIRV_PROFILE) != 0)
    {
        LOG_INFO("Shader cache SPIR-V profile mismatch for file ", m_HashFile.string());
        return false;
    }

    if (l_Header.contentHash != p_HashValue)
    {
        LOG_INFO("Shader cache hash mismatch for file ", m_HashFile.string());
        return false;
    }
    uint32_t l_StageCount = 0;
    l_File.read(reinterpret_cast<char*>(&l_StageCount), sizeof(uint32_t));
    m_CachedCodes.reserve(l_StageCount);
    for (uint32_t i = 0; i < l_StageCount; i++)
    {
        CachedCodes l_CachedCode;
        uint32_t l_Stage = 0;
        l_File.read(reinterpret_cast<char*>(&l_Stage), sizeof(uint32_t));
        l_CachedCode.stage = static_cast<VkShaderStageFlagBits>(l_Stage);
        uint32_t l_NameLength = 0;
        l_File.read(reinterpret_cast<char*>(&l_NameLength), sizeof(uint32_t));
        l_CachedCode.name.resize(l_NameLength);
        l_File.read(reinterpret_cast<char*>(l_CachedCode.name.data()), l_NameLength);
        uint32_t l_CodeSize = 0;
        l_File.read(reinterpret_cast<char*>(&l_CodeSize), sizeof(uint32_t));
        l_CachedCode.spirv.resize(l_CodeSize / sizeof(uint32_t));
        l_File.read(reinterpret_cast<char*>(l_CachedCode.spirv.data()), l_CodeSize);
        m_CachedCodes.push_back(std::move(l_CachedCode));
    }

    if (m_ExpectedStages != 0)
    {
        for (uint32_t l_Stage = 0; l_Stage < 32; l_Stage++)
        {
            const VkShaderStageFlagBits l_StageBit = static_cast<VkShaderStageFlagBits>(1 << l_Stage);
            if ((m_ExpectedStages & l_StageBit) != 0)
            {
                bool l_Found = false;
                for (const CachedCodes& l_CachedCode : m_CachedCodes)
                {
                    if (l_CachedCode.stage == l_StageBit)
                    {
                        l_Found = true;
                        break;
                    }
                }
                if (!l_Found)
                {
                    LOG_INFO("Shader cache missing expected stage: ", string_VkShaderStageFlagBits(l_StageBit));
                    return false;
                }
            }
        }
    }

    if (!m_ExpectedEntryPoints.empty())
    {
        for (const std::string& l_ExpectedEntry : m_ExpectedEntryPoints)
        {
            bool l_Found = false;
            for (const CachedCodes& l_CachedCode : m_CachedCodes)
            {
                if (l_CachedCode.name == l_ExpectedEntry)
                {
                    l_Found = true;
                    break;
                }
            }
            if (!l_Found)
            {
                LOG_INFO("Shader cache missing expected entry point: ", l_ExpectedEntry);
                return false;
            }
        }
    }

    return true;
}

void VulkanShader::saveCodeCache()
{
    if (m_Result.status != Result::COMPILED)
    {
        return;
    }

    std::filesystem::create_directories(m_HashFile.parent_path());

    const std::string l_TempFile = m_HashFile.string() + ".tmp";

    std::ofstream l_File{ l_TempFile, std::ios::binary };
    if (!l_File.is_open())
    {
        LOG_ERR("Failed to open shader cache file for writing: " + m_HashFile.string());
        return;
    }

    CacheHeader l_Header;
    const char* l_SlangVersion = s_SlangSessions[m_CompilationThread]->getBuildTagString();
    strncpy_s(l_Header.slangVersion, l_SlangVersion, sizeof(l_Header.slangVersion) - 1);
    strncpy_s(l_Header.spirvProfile, SPIRV_PROFILE, sizeof(l_Header.spirvProfile) - 1);

    size_t l_HashValue = m_Hash.getHash();
    hashCombine(l_HashValue, static_cast<size_t>(m_Optimize));
    hashCombine(l_HashValue, std::string(SPIRV_PROFILE));
    l_Header.contentHash = l_HashValue;

    l_File.write(reinterpret_cast<const char*>(&l_Header), sizeof(CacheHeader));

    slang::ProgramLayout* l_Layout = getLayout();
    for (uint32_t i = 0; i < l_Layout->getEntryPointCount(); i++)
    {
        slang::EntryPointLayout* l_EntryPoint = l_Layout->getEntryPointByIndex(i);
        const VkShaderStageFlagBits l_Stage = getVkStageFromSlangStage(l_EntryPoint->getStage());
        std::vector<uint32_t> l_Spirv = getSpirvForStage(l_Stage);
        if (m_Result.status == Result::FAILED)
        {
            LOG_ERR("Failed to get SPIR-V for shader stage during cache save -> ", m_Result.error);
            return;
        }
        CachedCodes l_CachedCode;
        l_CachedCode.stage = l_Stage;
        l_CachedCode.name = l_EntryPoint->getName();
        l_CachedCode.spirv = std::move(l_Spirv);
        m_CachedCodes.push_back(std::move(l_CachedCode));
    }
    
    const uint32_t l_StageCount = static_cast<uint32_t>(m_CachedCodes.size());
    l_File.write(reinterpret_cast<const char*>(&l_StageCount), sizeof(uint32_t));
    for (const CachedCodes& l_CachedCode : m_CachedCodes)
    {
        uint32_t l_Stage = static_cast<uint32_t>(l_CachedCode.stage);
        l_File.write(reinterpret_cast<const char*>(&l_Stage), sizeof(uint32_t));
        uint32_t l_NameLength = static_cast<uint32_t>(l_CachedCode.name.size());
        l_File.write(reinterpret_cast<const char*>(&l_NameLength), sizeof(uint32_t));
        l_File.write(reinterpret_cast<const char*>(l_CachedCode.name.data()), l_NameLength);
        uint32_t l_CodeSize = static_cast<uint32_t>(l_CachedCode.spirv.size() * sizeof(uint32_t));
        l_File.write(reinterpret_cast<const char*>(&l_CodeSize), sizeof(uint32_t));
        l_File.write(reinterpret_cast<const char*>(l_CachedCode.spirv.data()), l_CodeSize);
    }

    l_File.close();
    std::filesystem::rename(l_TempFile, m_HashFile);
}

void VulkanShader::loadModule(const std::string_view p_Filename, const std::string_view p_ModuleName)
{
    if (m_Result.status == Result::FAILED)
    {
        return;
    }

    std::ifstream l_File{std::string(p_Filename)};
    if (!l_File.is_open())
    {
        m_Result.error = "Failed to open shader file: " + std::string(p_Filename);
        m_Result.status = Result::FAILED;
        return;
    }

    const std::string l_ParentPath = std::filesystem::path{p_Filename}.parent_path().string();
    if (!l_ParentPath.empty())
    {
        addSearchPath(l_ParentPath);
    }

    std::stringstream l_Stream;
    l_Stream << l_File.rdbuf();
    l_File.close();

    loadModuleString(l_Stream.str(), p_ModuleName);
}

void VulkanShader::loadModuleString(const std::string_view p_Source, const std::string_view p_ModuleName)
{
    slang::IBlob* l_DiagnosticsBlob = nullptr;
    slang::IModule* l_Module = m_SlangSession->loadModuleFromSourceString(p_ModuleName.data(), p_ModuleName.data(), p_Source.data(), &l_DiagnosticsBlob);
    printBlob(l_DiagnosticsBlob);

    if (!l_Module)
    {
        m_Result.error = "Failed to load shader module: " + std::string(p_ModuleName);
        m_Result.status = Result::FAILED;
        return;
    }

    m_SlangComponents.push_back(l_Module);

    for (int32_t i = 0; i < l_Module->getDefinedEntryPointCount(); i++)
    {
        slang::IEntryPoint* l_EntryPoint = nullptr;
        if (SLANG_FAILED(l_Module->getDefinedEntryPoint(i, &l_EntryPoint)))
        {
            m_Result.error = "Failed to get entry point by index: " + std::to_string(i);
            m_Result.status = Result::FAILED;
            return;
        }
        m_SlangComponents.push_back(l_EntryPoint);
    }
}

void VulkanShader::linkAndFinalize()
{
    if (m_Result.status == Result::FAILED)
    {
        return;
    }

    slang::IBlob* l_DiagnosticsBlob = nullptr;
    if (SLANG_FAILED(m_SlangSession->createCompositeComponentType(m_SlangComponents.data(), m_SlangComponents.size(), &m_SlangProgram, &l_DiagnosticsBlob)))
    {
        printBlob(l_DiagnosticsBlob);
        m_Result.error = "Failed to link shader modules";
        m_Result.status = Result::FAILED;
    }

    m_SlangComponents.clear();
    m_Result.status = Result::COMPILED;
}

std::vector<uint32_t> VulkanShader::getSpirvForStage(const VkShaderStageFlagBits p_Stage)
{
    if (m_Result.status != Result::COMPILED && m_Result.status != Result::CACHED)
    {
        m_Result.error = "Shader compilation not finished";
        m_Result.status = Result::FAILED;
        return {};
    }

    for (const CachedCodes& l_CachedCode : m_CachedCodes)
    {
        if (l_CachedCode.stage == p_Stage)
        {
            return l_CachedCode.spirv;
        }
    }

    if (m_Result.status == Result::CACHED)
    {
        LOG_WARN("Shader entry point not found in cache for stage: ", string_VkShaderStageFlagBits(p_Stage), ". Falling back to runtime extraction.");
        compile(true);
    }

    uint32_t l_EntryPointIndex = UINT32_MAX;
    slang::IBlob* l_DiagnosticsBlob = nullptr;
    slang::ProgramLayout* l_Layout = m_SlangProgram->getLayout(0, &l_DiagnosticsBlob);
    printBlob(l_DiagnosticsBlob);

    const SlangStage l_SlangStage = getSlangStageFromVkStage(p_Stage);
    for (uint32_t i = 0; i < l_Layout->getEntryPointCount(); i++)
    {
        slang::EntryPointLayout* l_EntryPoint = l_Layout->getEntryPointByIndex(i);
        if (l_EntryPoint->getStage() == l_SlangStage)
        {
            l_EntryPointIndex = i;
            break;
        }
    }
    if (l_EntryPointIndex == UINT32_MAX)
    {
        m_Result.error = "Failed to find entry point for shader stage: " + std::to_string(p_Stage);
        m_Result.status = Result::FAILED;
        return {};
    }

    slang::IBlob* l_EntryPointBlob = nullptr;
    if (SLANG_FAILED(m_SlangProgram->getEntryPointCode(l_EntryPointIndex, 0, &l_EntryPointBlob, &l_DiagnosticsBlob)))
    {
        printBlob(l_DiagnosticsBlob);
        m_Result.error = "Failed to get SPIR-V for shader stage: " + std::to_string(p_Stage);
        m_Result.status = Result::FAILED;
        return {};
    }

    std::vector<uint32_t> l_SPIRV;
    l_SPIRV.resize(l_EntryPointBlob->getBufferSize() / sizeof(uint32_t));
    memcpy(l_SPIRV.data(), l_EntryPointBlob->getBufferPointer(), l_EntryPointBlob->getBufferSize());
    l_EntryPointBlob->release();

    return l_SPIRV;
}

std::vector<uint32_t> VulkanShader::getSpirvFromName(const std::string_view p_Name)
{
    if (m_Result.status != Result::COMPILED && m_Result.status != Result::CACHED)
    {
        m_Result.error = "Shader compilation not finished";
        m_Result.status = Result::FAILED;
        return {};
    }

    for (const CachedCodes& l_CachedCode : m_CachedCodes)
    {
        if (l_CachedCode.name == p_Name)
        {
            return l_CachedCode.spirv;
        }
    }

    if (m_Result.status == Result::CACHED)
    {
        LOG_WARN("Shader entry point not found in cache for name: ", p_Name, ". Falling back to runtime extraction.");
        compile(true);
    }

    uint32_t l_EntryPointIndex = 0;
    slang::IBlob* l_DiagnosticsBlob = nullptr;
    slang::ProgramLayout* l_Layout = m_SlangProgram->getLayout(0, &l_DiagnosticsBlob);
    printBlob(l_DiagnosticsBlob);

    for (uint32_t i = 0; i < l_Layout->getEntryPointCount(); i++)
    {
        slang::EntryPointLayout* l_EntryPoint = l_Layout->getEntryPointByIndex(i);
        if (l_EntryPoint->getName() == p_Name)
        {
            l_EntryPointIndex = i;
            break;
        }
    }

    slang::IBlob* l_EntryPointBlob = nullptr;
    if (SLANG_FAILED(m_SlangProgram->getEntryPointCode(l_EntryPointIndex, 0, &l_EntryPointBlob, &l_DiagnosticsBlob)))
    {
        printBlob(l_DiagnosticsBlob);
        m_Result.error = "Failed to get SPIR-V for shader entry point: " + std::string(p_Name);
        m_Result.status = Result::FAILED;
        return {};
    }

    std::vector<uint32_t> l_SPIRV;
    l_SPIRV.resize(l_EntryPointBlob->getBufferSize() / sizeof(uint32_t));
    memcpy(l_SPIRV.data(), l_EntryPointBlob->getBufferPointer(), l_EntryPointBlob->getBufferSize());
    l_EntryPointBlob->release();

    return l_SPIRV;
}

VkShaderModule VulkanShaderModule::operator*() const
{
    return m_VkHandle;
}

slang::ProgramLayout* VulkanShader::getLayout() const
{
    if (m_Result.status != Result::COMPILED)
    {
        throw std::runtime_error("Could not obtain shader layout, compilation not finished");
    }

    return m_SlangProgram->getLayout(0, nullptr);
}

bool VulkanShader::buildSession(const ThreadID p_CompilationThread, const bool p_Optimize)
{
    if (!s_SlangSessions.contains(p_CompilationThread))
    {
        slang::IGlobalSession* l_SlangGlobalSession = nullptr;
        if (SLANG_FAILED(slang::createGlobalSession(&l_SlangGlobalSession)))
        {
            m_Result.error = "Failed to create Slang global session";
            m_Result.status = Result::FAILED;
            return false;
        }
        s_SlangSessions[p_CompilationThread] = l_SlangGlobalSession;
    }

    slang::SessionDesc l_SessionDesc = {};

    slang::TargetDesc l_TargetDesc = {};
    l_TargetDesc.format = SLANG_SPIRV;
    l_TargetDesc.profile = s_SlangSessions[p_CompilationThread]->findProfile(SPIRV_PROFILE);

    l_SessionDesc.targetCount = 1;
    l_SessionDesc.targets = &l_TargetDesc;

    std::array<slang::CompilerOptionEntry, 3> l_Options;
    l_Options[0] = {slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}};
    l_Options[1] = {slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, static_cast<int>(p_Optimize ? SLANG_OPTIMIZATION_LEVEL_HIGH : SLANG_OPTIMIZATION_LEVEL_NONE), 0, nullptr, nullptr}};
    l_Options[2] = {slang::CompilerOptionName::DebugInformation, {slang::CompilerOptionValueKind::Int, static_cast<int>(p_Optimize ? SLANG_DEBUG_INFO_LEVEL_NONE : SLANG_DEBUG_INFO_LEVEL_MAXIMAL), 0, nullptr, nullptr}};

    l_SessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(l_Options.size());
    l_SessionDesc.compilerOptionEntries = l_Options.data();

    std::vector<const char*> l_SearchPaths;
    l_SearchPaths.reserve(m_SearchPaths.size());
    for (const std::string& l_Path : m_SearchPaths)
    {
        l_SearchPaths.push_back(l_Path.c_str());
    }

    l_SessionDesc.searchPathCount = static_cast<uint32_t>(l_SearchPaths.size());
    l_SessionDesc.searchPaths = l_SearchPaths.data();

    std::vector<slang::PreprocessorMacroDesc> l_Macros;
    l_Macros.reserve(m_Macros.size());
    for (const MacroDef& l_Macro : m_Macros)
    {
        slang::PreprocessorMacroDesc l_Desc = {};
        l_Desc.name = l_Macro.name.c_str();
        l_Desc.value = l_Macro.value.c_str();
        l_Macros.push_back(l_Desc);
    }

    l_SessionDesc.preprocessorMacroCount = static_cast<uint32_t>(l_Macros.size());
    l_SessionDesc.preprocessorMacros = l_Macros.data();

    if (SLANG_FAILED(s_SlangSessions[p_CompilationThread]->createSession(l_SessionDesc, &m_SlangSession)))
    {
        m_Result.error = "Failed to create Slang session";
        m_Result.status = Result::FAILED;
        return false;
    }
    return true;
}

void VulkanShaderModule::free()
{
    if (m_VkHandle != VK_NULL_HANDLE)
    {
        const VulkanDevice& l_Device = VulkanContext::getDevice(getDeviceID());

        l_Device.getTable().vkDestroyShaderModule(l_Device.m_VkHandle, m_VkHandle, nullptr);
        LOG_DEBUG("Freed shader module (ID: ", m_ID, ")");
        m_VkHandle = VK_NULL_HANDLE;
    }
}

VulkanShaderModule::VulkanShaderModule(const ResourceID p_Device, const VkShaderModule p_Handle, const VkShaderStageFlagBits p_Stage)
    : VulkanDeviceSubresource(p_Device), m_VkHandle(p_Handle), m_Stage(p_Stage) {}

void VulkanShader::addSearchPath(const std::string& p_Path)
{
    m_SearchPaths.insert(p_Path);
}

void VulkanShader::addCacheDependency(const std::filesystem::path& p_File)
{
    if (m_UseCache)
    {
        m_Hash.addFile(p_File);
    }
}

void VulkanShader::addCacheDependencyFolder(const std::filesystem::path& p_Folder, bool p_Recursive)
{
    if (m_UseCache)
    {
        m_Hash.addFolder(p_Folder, p_Recursive);
    }
}
