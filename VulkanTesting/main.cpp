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
#include <fstream>
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
    std::vector<VkImageView> swapChainImageViews; // they describe how to access images and which parts of them to access
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout; // used to change behaviour of shaders after pipeline is created
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers; // Binds the attachments for input to the renderPass
    VkCommandPool commandPool; // a commandPool manages memory to store command buffers
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore; // to signal that an image has been aquired from the chain and ready to be rendered to
    VkSemaphore renderFinishedSemaphore; // to signal that the image has finished rederering and can be presented
    
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
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSemaphores();
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
    
    static std::vector<char> readFile(const std::string& filename) {
        std::cout << "Loading " << filename << std::endl;
        // ate: read at the end, so the read position determines file size; binary: avoid text transformations
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file");
        }
        
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        
        // seek back to the beginning
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        
        return buffer;
    }
    
    void createSemaphores() {
        // it does not take much...
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS
            ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphores");
        }
    }
    
    void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        /* Level means primary or secondary command buffer:
         - VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
         - VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers. Useful to reuse common operations from primary command buffers
         */
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }
        
        for (size_t i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            
            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording a command buffer");
            }
            
            // now we start recording the commands. Methods that start with vkCmd are commands being recorded.
            // we have to begin the render pass first thing:
            
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChainExtent;
            // what to clear to when vk_attachment_load_op_clear. Black it is:
            VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;
            
            // final paramete tells if the commands will execute on secondary command buffers (VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS) or not (VK_SUBPASS_CONTENTS_INLINE)
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
            
            // finally, we are DRAWING THE TRIANGLE.
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
            
            // finish recording
            vkCmdEndRenderPass(commandBuffers[i]);
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record a command buffer");
            }
        }
        
    }
    
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        // Commands are submited to one type of queue. We're drawing, so we requrie the graphics one
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
        
    }
    
    void createFramebuffers() {
        // One per image in the swapchain
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = { swapChainImageViews[i] };
               
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;
            
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        
        }
    }
    
    void createRenderPass() {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        // no multisampling
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        // what to do with the attachment when the rederpass begins and loads it and ends and saves it
        /*
         - VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the at- tachment
         - VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start
         - VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don’t care about them
         */
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // Above applies to color and depth data. Below to stencil, which we're not using
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // format when we load
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // format when we save. Has to do with what we're doing next with it
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // this binds to location(layout=0) in the fragment shader
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = & colorAttachmentRef;
        
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // if srcsubpass is this value, it means "before the subpass"
        dependency.dstSubpass = 0; // our subpass is index 0
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //wait for this to happen ...
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //.. before outputing to this stage...
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // .. for this operations
        
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create renderpass");
        }
    }
    
    void createGraphicsPipeline() {
        /*
         Creating a graphics pipeline requires a bunch of objects:
         - Shader stages: the shader modules that define the functionality of the programmable stages of the graphics pipeline
         - Fixed-function state: all of the structures that define the fixed-function stages of the pipeline, like input assembly, rasterizer, viewport and color blending
         - Pipeline layout: the uniform and push values referenced by the shader that can be updated at draw time
         - Render pass: the attachments referenced by the pipeline stages and their usage
         */
        
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");
        
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
        
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; //entrypoint
        
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // Bindings define spacing between data (how do we separate itemps in the vertex list or in the instance list) and wheteher this is a list of vertices or instances.
        // Since this time we generate the vertices on the vertex shader, we will pass nothing.
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        // Attributes of the vertices when passed to the shader.
        // This time, no vertices, no attributes.
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        /* what kind of geometry to draw from the vertices (how to interpret the vertex list)
         -  VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
         - VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without
         reuse
         - VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: the end vertex of every line is used
         as start vertex for the next line
         - VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: triangle from every 3 vertices
         without reuse
         - VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: the second and third vertex
         of every triangle are used as first two vertices of the next triangle
         */
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // for _STRIP topologies, interpret special values of 0xFFFF or 0xFFFFFFFF as a break up on the list
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        // The region of the framebuffer the output will be rendered to
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        // While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will actually be stored. Can be seen as something similar to a mask region
        // We will not cut out anything
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        
        // Need to combine viewport and scissor into viewportState
        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        // The rasterizer takes geometry and makes it fragments
        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        // When false, fragments outside the near and far planes are discarded. When true, they are clamped to that range, but this requires a GPU feature
        rasterizer.depthClampEnable = VK_FALSE;
        /*
         - VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
         - VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
         - VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
         */
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        // whether or not to cull faces and how (discard based on where they're pointing to)
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        // how do we consider what's front or back facing
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        // depth values can be altered by constants. We won't
        rasterizer.depthBiasEnable = VK_FALSE;
        
        // antialias by combining the result of fragment shaders from multiple polygons that end up on the same pixel
        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        // we won't use it. If we wanted to, we'd require a GPU feature
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        // Color blending is combining the output from the fragment shader with the color already in the framebuffer. We can mix the colors into a new one or combine them using a bitwise operation
        // We need two structures to configure color blending
        
        // Configuration per attached framebuffer:
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        // all colors, no mask
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        // Configuration for all the framebuffers
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        // This are values referenced by shaders that can be updated at draw time
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // wont' need anything for now...
        
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
        
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        // next parameters would be useful if we wanted to create derivative graphics pipelines (like optimizing from a common parent, etc)
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;
        
        
        // second param is a pointer to the pipeline cache, which we dont' have
        // third parameter is the amount of pipelineCreateInfos that we'll input, since this function allows for creating multiple pipielines at once
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
        
        // it's ok that shader modules' lifetime is local because they are only needed at pipeline creation
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
    }
    
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        // size of code is in bytes but pointer is a uint32_t instead of char
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }
        
        return shaderModule;
    }
    
    void createImageViews() {
        // We need as many views as there are images
        swapChainImageViews.resize(swapChainImages.size());
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 2d texture/canvas. This could be 1d, 3d, cubemap...
            createInfo.format = swapChainImageFormat;
            //we can play around with color channels, but we'll stick to default mapping:
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            //the following attributes describe the image's purpose. This time, it's just a color target
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            // If it were stereoscopic, we would create one view per layer of an image
            createInfo.subresourceRange.layerCount = 1;
            
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views");
                };
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
            drawFrame();
        }
    }
    
    void cleanup() {
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
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
    
    void drawFrame() {
        /*
         - Acquire an image from the swap chain
         - Execute the command buffer with that image as attachment in the frame-
         buffer
         - Return the image to the swap chain for presentation
         
         Only problem: asyncronous functions. Enter semaphores
         */
        
        // Acquire
        uint32_t imageIndex; // index in swapChainImages of the swapchain image (VkImage) that has been acquired
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        
        //Execute
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore}; // wait for these ...
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // ... at these stages
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex]; // the command buffer associated with the image we acquired
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore}; // which semaphores to signal after finishing execution
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer");
        }
        
        //return to the swapchain
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        // present to screen!
        vkQueuePresentKHR(presentQueue, &presentInfo);
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
