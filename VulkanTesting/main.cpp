#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <optional> //requires c++17
#include <set>
#include <cstdint>
#include "helper_extensions.h"

const int WIDTH = 800;
const int HEIGHT = 600;


// Validation layers are for debugging purposes
const std::vector<const char*> validationLayers = {
    // standard validation that comes with the SDK
    "VK_LAYER_KHRONOS_validation"
};

// Here we list the device extensions we require
const std::vector<const char*> deviceExtensions {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME // Not all devices can present. Thus, swapchains are an extension provided by the device
};

// If not in debug mode, don't enable the validation layers
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {

    public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
        
    private:
    GLFWwindow* window; // GFLW manages windowing. This is a pointer to our window
    VkInstance instance; // The instance connects the app and the Vulkan library
    VkDebugUtilsMessengerEXT debugMessenger; // A callback for debugging purposes
    VkSurfaceKHR surface; // A surface is where images actually get rendered to. It is an abstract representation that will be backed by whatever windowing system we're using (GLFW in our case)
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // handle to the phyisical device
    VkDevice device; // This will be the logical device
    VkQueue graphicsQueue; // Queues are created along the logical device but we need somewhere to store a handler to them
    VkQueue presentQueue; // Queues are created along the logical device but we need somewhere to store a handler to them
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily; // Drawing
        std::optional<uint32_t> presentFamily; // Presenting to surfaces
        
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities; // max/min number of images, max/min witdh and height of images
        std::vector<VkSurfaceFormatKHR> formats; // pixel format and color space
        std::vector<VkPresentModeKHR> presentModes; // available presentation modes -- like double buffering, triple buffering...
    };
    
    void initVulkan() {
        printVulkanSupportedExtensions();
        printGlfwRequiredExtensions();
        checkGlfwRequiredExtensionsAvailable();
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
    }
    
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount !=0) {
            details.formats.resize(formatCount); // properly allocate the vector
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount); // properly allocate the vector
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        
        return details;
    }
    
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // If SRGB available, use it
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        
        // Else, return whatever the first format available is
        return availableFormats[0];
    }
    
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        /* Four modes are available:
         - VK_PRESENT_MODE_IMMEDIATE_KHR: Images sent right away to the screen, which may result in tearing.
         - VK_PRESENT_MODE_FIFO_KHR: Queue mode, where the display takes an image from the front of the queue on refresh while the app pushes images to the back. App has to wait if queue is full. Similar to VSYNC.
         - VK_PRESENT_MODE_FIFO_RELAXED_KHR: Same but if there's no image on refresh, next image will get drawn once is ready. Like "variable vsync", which might result in tearing
         - VK_PRESENT_MODE_MAILBOX_KHR: Like FIFO but discard images from the queue if the queue is full when the app submits a new one. This discards frames in that case. Similar to triple buffering.
         
         Only VK_PRESENT_MODE_FIFO_KHR is guaranteed to be available.
         */
        
        for (const auto& availablePresentMode : availablePresentModes) {
            // Should we have triple buffer, use it
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        
        // else, let's stick with the guaranteed one
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        // This is the resolution of the images in the swap chain. Should be equal to the resolution of the window.
        // The ranges avaiable are in VkSurfaceCapabilitiesKHR.currentExtent.
        // Some window managers store there a special value equal to the maximum uint32_t.
        
        if (capabilities.currentExtent.width != UINT32_MAX) {
            // if the current extent has no special value, use it because it's automatched
            return capabilities.currentExtent;
        } else {
            // use the dimensions we defined for the window within the min/max bounds
            VkExtent2D actualExtent = {WIDTH, HEIGHT};
            actualExtent.width = std::max(capabilities.minImageExtent.width,
                                          std::min(capabilities.maxImageExtent.width,
                                                   actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height,
                     actualExtent.height));
            return actualExtent;
        }
    }
    
    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        
        // theres a min and max number of images on the swap chain, it varies by implementation
        // If we stick to the minimum, we'll have to wait a lot, so we require at lest one one than the min
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        
        // there's a special value 0 for the max which means "no max". Either way, let's try not to get over the max if there's any
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // Always 1 unless stereoscopic
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Color attachment: basically, draw to this
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // if we don't want to transform (rotate, etc), use the current transform, which does nothing
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Ignore alpha channel
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // Don't write to pixels covered by other windows
        createInfo.oldSwapchain = VK_NULL_HANDLE; // This would be used if we wanted to recreate the swap chain, we would point to the old one
        
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        
        // Some GPUs will have the same family for graphics and presentation, some will have different families
        if (indices.graphicsFamily != indices.presentFamily) {
            // If they are different, use concurrent sharing more. An image is owned by either family and ownership transfers explicitly. Best performance
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            // If they are the same, let them have exclusive access because they won't be competing anyway
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // optional
            createInfo.pQueueFamilyIndices = nullptr; // optional
        }
        
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Could not create swap chain");
        }
        
        
        // imageCount was the minimum number of images we requested, but the device can create any number above that. Now we'll get the actual number
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        // then we'll resize the vector
        swapChainImages.resize(imageCount);
        // and finally store th handlers to those images
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        
        //store format and extent in member variables for future use
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }
    
    void createSurface() {
        // Creating an instance VkSurfaceKHR is platform dependant (while VkSurfaceKHR itself is not). We could use platform-specific methods to create it or, since we're using GLFW, use its own abstractions that will deal with that
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
    }
    
    void createLogicalDevice() {
        
        // Create the actual logical queues we're interested in using
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        
        // Since we're going to need more than one queue...
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
        float queuePriority = 1.0f;
        
        for (uint32_t queueFamily: uniqueQueueFamilies) {
            // Define each VkDeviceQueueCreateInfo...
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            // ...and push them to the vector
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        // Enable the GPU features we want to use. Right now we won't as for any
        VkPhysicalDeviceFeatures deviceFeatures = {};
        
        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        if (enableValidationLayers) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            deviceCreateInfo.enabledLayerCount = 0;
        }
        
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }
        
        // This stores a handle to a graphics queue in graphicsQueue. The 0 is the index of the queue within the family. A device can potentially provice multiple queues for the same family. In this case we're only interested in one, so the first one suffices.
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
    

    void printPhysicalDeviceInfo(VkPhysicalDevice_T *const &device) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        std::ios_base::fmtflags f( std::cout.flags()); // Save std flags to restore them later. Changing output to hex is stateful
        std::cout << "DeviceId=" << deviceProperties.deviceID << ". VendorId=0x" << std::hex << deviceProperties.vendorID << ". API version=" << std::dec << deviceProperties.apiVersion << std::endl;
        std::cout.flags(f);
    }
    
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        
        uint32_t queueFamilyCount  = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        int i = 0;
        for (const auto& queueFamily: queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            i++;
        }
        
        return indices;
    }
    
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find Vulkan GPUs");
        } else {
            std::cout << "GPUs found: " << deviceCount << std::endl;
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        for (const auto& device: devices) {
            if (isDeviceSuitable(device)) {
                std::cout << "Found Device: ";
                printPhysicalDeviceInfo(device); // Restore std state
                physicalDevice = device; //This effectively makes the last GPU found on multi-GPU systems the one chosen
            }
        }
        
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to select a suitable GPU");
        } else {
            std::cout << "Chosen Device: ";
            printPhysicalDeviceInfo(physicalDevice);
        }
    }
    
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        
        bool swapChainAdequate = false; // Assume the worse
        if (extensionsSupported) { // important to query about swap chain support if and only if the extension is available
            SwapChainSupportDetails swapChainSupoprt = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupoprt.formats.empty() && !swapChainSupoprt.presentModes.empty();
        }
        
        return indices.graphicsFamily.has_value() && indices.presentFamily.has_value() && extensionsSupported && swapChainAdequate; // We require a graphics queue, a presentation queue and proper swapchain support
    }
    
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        
        for (const auto& extension : availableExtensions) {
            // the idea here is that we'll erase all required extensions once they're found.
            requiredExtensions.erase(extension.extensionName);
        }
        
        // if the set is empty, then all the required extensions where found
        return requiredExtensions.empty();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // Wait for events that happen to the window, such as closing it
        }
    }
    
    void cleanup() {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    void initWindow() {
        glfwInit(); // Initializes the GLFW library
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // This prevents GLFW from creating an OpenGL context, because we're going to use Vulkan instead
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Make the windw non-resizable because we don't want to deal with its implications for now
        
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr); // Create a window of WIDHTxHEIGHT, with "Vulkan" as its title. First nullprt is for specifying a monitor but we go for default, second one is related only to OpenGL
    }
    
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers required but not available on this system");
        }
        
        VkApplicationInfo appInfo = {}; // We provide some info to the graphics driver about our app. Not mandatory but can help the driver with app or engine-specific optimizations
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Many structs in Vulkan have this field, it's mandatory
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        VkInstanceCreateInfo createInfo = {}; // Mandatory struct indicating global (as in program-wide and as opposed to device-specific) extensions and validation layers (for debugging)
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo; // points to the VkApplicationInfo struct above
        
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }
        
        /*
         We centralized getting all the required extensions because not only GLFW does require extensions.
         The bool parameter to listRequiredExtensions means whether or not we're in debug mode, which is equivalent to have validation layers enabled.
         We then pass the values to the VkInstanceCreateInfo struct.
         */
        std::vector<const char*> extensions = listRequiredExtensions(enableValidationLayers);
        createInfo.enabledExtensionCount = (uint32_t) extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
        
        /* Actually create the instance
         The general pattern that object creation function parameters in Vulkan follow is:
         • Pointer to struct with creation info
         • Pointer to custom allocator callbacks, unused for now (nullptr)
         • Pointer to the variable that stores the handle to the new object
         
         Nearly all Vulkan functions return a value of type VkResult that is either VK_SUCCESS or an error code
         */
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance");
        }
    }
    
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | // Diagnostic messages
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | // Warnings
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;    // Errors
        createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |    // Event not related to performance or violation of specs
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | // Spec violated
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Potential non-optimal use
        createInfo.pfnUserCallback = debugCallback; // Pointer to the actual callback function
        createInfo.pUserData = nullptr; // Used if we'd like to push custom data to the  callback.
    }
    
    void setupDebugMessenger() {
        if (!enableValidationLayers) return;
        
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to setup debug messenger");
        }
    }
    
    //This is because the required function comes from extensions, so it's not loaded automatically. This looks for the function address and runs it, returning an error if the extension is nout found.
    //Made static so it's not tied to the lifetime of the class
    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    
    //Same as above but for destruction. Important that this is static or we'll race in the cleanup
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, debugMessenger, pAllocator);
        }
    }
    
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        
        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            
            if (!layerFound) {
                return false;
            }
        }
        
        return true;
    }
    
    // VKAPI_ATTR and VKAPI_CALL are there to ensure this is callable by Vulkan
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData){
        
        std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
        
        return VK_FALSE; // does the call that triggered this need to be aborted?
    }
    
};

int main() {
    HelloTriangleApplication app;
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
