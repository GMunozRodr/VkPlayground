#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <slang/slang.h>
#include <Volk/volk.h>

#include "utils/identifiable.hpp"

class VulkanDevice;

class ShaderHash
{
public:
    ShaderHash() = default;

    void addFile(const std::filesystem::path& p_File);
    void addFolder(const std::filesystem::path& p_Folder, bool p_Recursive = true);
    void addString(const std::string& p_Content);
    void addMacro(const std::string& p_Name, const std::string& p_Value);

    [[nodiscard]] size_t getHash() const;

private:
    std::vector<std::string> m_Data;

    mutable size_t m_HashValue = 0;
};

class VulkanShader
{
public:
    struct MacroDef
    {
        std::string name;
        std::string value;
    };

    struct Result
    {
        enum Status : uint8_t { NOT_READY, CACHED, FAILED, COMPILED };

        Status status = NOT_READY;
        std::string error;
    };

    static void reset(VulkanShader& p_Shader);
    static void reinit(VulkanShader& p_Shader, ThreadID p_CompilationThread, bool p_Optimize, std::span<const MacroDef> p_Macros = {});

    VulkanShader() = default;
    explicit VulkanShader(ThreadID p_CompilationThread, bool p_Optimize = true, std::span<const MacroDef> p_Macros = {});
    ~VulkanShader();
    VulkanShader(const VulkanShader&) = delete;
    VulkanShader(VulkanShader&&) noexcept;

    void enableCache(std::filesystem::path p_CacheFile);
    void setExpectedStages(VkShaderStageFlags p_Stages, std::span<std::string> p_EntryPoints = {});

    void addModule(std::string_view p_Filename, std::string_view p_ModuleName);
    void addModuleString(std::string_view p_Source, std::string_view p_ModuleName);

    void addCacheDependency(const std::filesystem::path& p_File);
    void addCacheDependencyFolder(const std::filesystem::path& p_Folder, bool p_Recursive = true);

    void compile(bool p_ForceCompile = false);

    [[nodiscard]] const Result& getStatus() const { return m_Result; }
    [[nodiscard]] std::vector<uint32_t> getSpirvForStage(VkShaderStageFlagBits p_Stage);
    [[nodiscard]] std::vector<uint32_t> getSpirvFromName(std::string_view p_Name);
    [[nodiscard]] const std::vector<MacroDef>& getMacros() const { return m_Macros; }
    [[nodiscard]] slang::ProgramLayout* getLayout() const;

    void addSearchPath(const std::string& p_Path);

    [[nodiscard]] size_t getHash() const { return m_Hash.getHash(); }

private:
    struct ModuleData
    {
        enum SourceType : uint8_t { FILE, STRING };

        std::string data;
        SourceType type;
        std::string name;
    };

    struct CachedCodes
    {
        VkShaderStageFlagBits stage;
        std::string name;
        std::vector<uint32_t> spirv;
    };

    std::vector<ModuleData> m_Modules;
    std::vector<CachedCodes> m_CachedCodes;
    VkShaderStageFlags m_ExpectedStages = 0;
    std::vector<std::string> m_ExpectedEntryPoints;

    ShaderHash m_Hash{};

    bool m_UseCache = false;
    std::filesystem::path m_HashFile;

    static constexpr uint32_t CACHE_VERSION = 1;
    static constexpr uint32_t CACHE_MAGIC = 0x53504956; // "SPIV"
    static constexpr const char* SPIRV_PROFILE = "spirv_1_5";

    struct CacheHeader
    {
        uint32_t magic = CACHE_MAGIC;
        uint32_t version = CACHE_VERSION;
        char slangVersion[32] = {};
        char spirvProfile[16] = {};
        size_t contentHash = 0;
    };

    bool tryLoadCachedCode(size_t p_HashValue);
    void saveCodeCache();

private:
    void loadModule(std::string_view p_Filename, std::string_view p_ModuleName);
    void loadModuleString(std::string_view p_Source, std::string_view p_ModuleName);

    void linkAndFinalize();

    bool buildSession(ThreadID p_CompilationThread, bool p_Optimize);

    std::vector<MacroDef> m_MacroDefs;

    std::vector<MacroDef> m_Macros;
    std::unordered_set<std::string> m_SearchPaths;

    Result m_Result;

    inline static std::unordered_map<ThreadID, slang::IGlobalSession*> s_SlangSessions;

    slang::ISession* m_SlangSession = nullptr;

    std::vector<slang::IComponentType*> m_SlangComponents;
    slang::IComponentType* m_SlangProgram = nullptr;

    bool m_Optimize = false;
    ThreadID m_CompilationThread = 0;

    friend class VulkanShaderModule;
};

class VulkanShaderModule final : public VulkanDeviceSubresource
{
public:
    VkShaderModule operator*() const;

    [[nodiscard]] VkShaderStageFlagBits getStage() const { return m_Stage; }

private:
    void free() override;

    VulkanShaderModule(ResourceID p_Device, VkShaderModule p_Handle, VkShaderStageFlagBits p_Stage);

    VkShaderModule m_VkHandle = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_Stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

    friend class VulkanDevice;
    friend class VulkanPipeline;
    friend struct VulkanPipelineBuilder;
};
