#pragma once

#include <Vulkan/vk_enum_string_helper.h>
#include "utils/logger.hpp"

#ifdef _DEBUG
    #define VULKAN_TRY(call)                                                                    \
        do {                                                                                    \
            if (const VkResult l_Result = call; l_Result != VK_SUCCESS)                         \
            {                                                                                   \
                LOG_ERR("Vulkan error at ", __FILE__, ":", __LINE__, " - ", string_VkResult(l_Result)); \
                throw std::runtime_error("Vulkan error at " + std::string(__FILE__) + ":" + std::to_string(__LINE__) + " - " + std::string(string_VkResult(l_Result))); \
            }                                                                                   \
        } while (false)
#else
    #define VULKAN_TRY(call) call
#endif