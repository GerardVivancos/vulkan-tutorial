#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>

const int WIDTH = 800;
const int HEIGHT = 600;

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
    
    void initVulkan() {
        printVulkanSupportedExtensions();
        printGlfwRequiredExtensions();
        checkGlfwRequiredExtensionsAvailable();
        createInstance();
    }
    
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // Wait for events that happen to the window, such as closing it
        }
    }
    
    void cleanup() {
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
        /* Now, in order to deal with GLFW windowing we need extensions because Vulkan is platform agnostic.
         GLFW provides a method that returns the list of Vulkan extensions it requires and receives a pointer to an uint which it will use to write the number of these extensions.
         We wrap this method so we can use it around.
         We then pass the values to the VkInstanceCreateInfo struct.
         */

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = listGlfwRequiredExtensions(&glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0; // Won't use validation layers now
        
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

    const std::vector<VkExtensionProperties> listVulkanSupportedExtensions(uint32_t* count = nullptr) {
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

    const char** listGlfwRequiredExtensions(uint32_t* count = nullptr) {
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
