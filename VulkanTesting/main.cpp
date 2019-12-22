#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <optional> //requires c++17
#include "helper_extensions.h"

const int WIDTH = 800;
const int HEIGHT = 600;


// Validation layers are for debugging purposes
const std::vector<const char*> validationLayers = {
    // standard validation that comes with the SDK
    "VK_LAYER_KHRONOS_validation"
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
    
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
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
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        
        // Enable the GPU features we want to use. Right now we won't as for any
        VkPhysicalDeviceFeatures deviceFeatures = {};
        
        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = 0;
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
        return indices.graphicsFamily.has_value(); // We require a graphics queue
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // Wait for events that happen to the window, such as closing it
        }
    }
    
    void cleanup() {
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
