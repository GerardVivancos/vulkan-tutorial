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
    std::vector<VkExtensionProperties> extensions = listVulkanSupportedExtensions(&extensionCount);
    std::cout << "Available extensions: " << extensionCount << std::endl;
    // "const auto&" lets us access the values without a copy (&), and prevents their accidental modification (const)
    for (const auto& extension: extensions) {
        std::cout << "--\t" << extension.extensionName << std::endl;
    }
}

const char** listGlfwRequiredExtensions(uint32_t* count) {
    // GLFW provides a method that returns the list of Vulkan extensions it requires and receives a pointer to an uint which it will use to write the number of these extensions.
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    if (count != nullptr) {
        *count = glfwExtensionCount;
    }
    return glfwExtensions;
}

void printGlfwRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = listGlfwRequiredExtensions(&glfwExtensionCount);
    std::cout << "Required extensions for GLFW: " << glfwExtensionCount << std::endl;
    for (int i = 0; i < glfwExtensionCount; i++)  {
        std::cout << "**\t" << glfwExtensions[i] << std::endl;
    }
}

bool checkGlfwRequiredExtensionsAvailable() {
    uint32_t glfwExtensionCount = 0;
    uint32_t vulkanExtensionCount = 0;
    uint32_t matchingExtensions = 0;
    const char** glfwExtensions = listGlfwRequiredExtensions(&glfwExtensionCount);
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
