//
//  helper_extensions.hpp
//  VulkanTesting
//
//  Created by garbage on 16/12/2019.
//  Copyright © 2019 garbage. All rights reserved.
//

#ifndef helper_extensions_h
#define helper_extensions_h
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#endif /* helper_extensions_h */

const std::vector<VkExtensionProperties> listVulkanSupportedExtensions(uint32_t* count = nullptr);
void printVulkanSupportedExtensions();
const char** listGlfwRequiredExtensions(uint32_t* count = nullptr);
void printGlfwRequiredExtensions();
bool checkGlfwRequiredExtensionsAvailable();
