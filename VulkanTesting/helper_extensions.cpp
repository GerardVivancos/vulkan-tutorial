//
//  helper_extensions.cpp
//  VulkanTesting
//
//  Created by garbage on 16/12/2019.
//  Copyright © 2019 garbage. All rights reserved.
//
#include "helper_extensions.h"

const std::vector<VkExtensionProperties> listVulkanSupportedExtensions(uint32_t* count) {
    uint32_t extensionCount = 0;

    // First call is so we can know the count and initialize a vector this big
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);

    // Actually retrieve the extension list
    // First argument is for filtering, not doing that now.
    // vector::data is a pointer to the underlying data array (http://www.cplusplus.com/reference/vector/vector/data/)
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    if (count != nullptr) {
        *count = extensionCount;
    }
    return extensions;
}

void printVulkanSupportedExtensions() {
    uint32_t extensionCount = 0;
    const std::vector<VkExtensionProperties> extensions = listVulkanSupportedExtensions(&extensionCount);
    std::cout << "Available extensions: " << extensionCount << std::endl;
    // "const auto&" lets us access the values without a copy (&), and prevents their accidental modification (const)
    for (const auto& extension: extensions) {
        std::cout << "--\t" << extension.extensionName << std::endl;
    }
}

const std::vector<const char*> listGlfwRequiredExtensions(uint32_t* count) {
    // In order to deal with GLFW windowing we need extensions because Vulkan is platform agnostic. GLFW provides a method that returns the list of Vulkan extensions it requires and receives a pointer to an uint which it will use to write the number of these extensions.
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    if (count != nullptr) {
        *count = glfwExtensionCount;
    }
    
    // This constructor expects a range: [first, last).
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    return extensions;
}

void printGlfwRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const std::vector<const char*> glfwExtensions = listGlfwRequiredExtensions(&glfwExtensionCount);
    std::cout << "Required extensions for GLFW: " << glfwExtensionCount << std::endl;
    for (int i = 0; i < glfwExtensionCount; i++)  {
        std::cout << "**\t" << glfwExtensions[i] << std::endl;
    }
}

bool checkGlfwRequiredExtensionsAvailable() {
    uint32_t glfwExtensionCount = 0;
    uint32_t vulkanExtensionCount = 0;
    uint32_t matchingExtensions = 0;
    const std::vector<const char*> glfwExtensions = listGlfwRequiredExtensions(&glfwExtensionCount);
    const std::vector<VkExtensionProperties> vulkanExtensions = listVulkanSupportedExtensions(&vulkanExtensionCount);

    for (int i = 0; i < glfwExtensionCount; i++) {
        for (const auto& vulkanExtension: vulkanExtensions) {
            if (0 == strcmp(glfwExtensions[i], vulkanExtension.extensionName)) {
            matchingExtensions++;
            }
        }
    }

    // Do all the required extensions match?
    return matchingExtensions == glfwExtensionCount;
}

const std::vector<const char*> listDebugRequiredExtensions() {
    std::vector<const char*> extensions({VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
    return extensions;
}

const std::vector<const char*> listRequiredExtensions(bool debug) {
    std::vector<const char*> extensions = listGlfwRequiredExtensions();
    
    if (debug) {
        std::vector<const char*> debugExtensions = listDebugRequiredExtensions();
        
        // insert at the end of extensions what starts at the beginning of debugExtensions and ranges up to the end of debugExtensions, excluded
        extensions.insert(extensions.end(), debugExtensions.begin(), debugExtensions.end());
    }
    
    return extensions;
}
