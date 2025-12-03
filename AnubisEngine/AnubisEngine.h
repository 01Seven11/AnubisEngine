// the beginnings of this tutorial is based off of this website as reference
// https://docs.vulkan.org/tutorial/latest/
#pragma once
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "GeneratedShapes.h"
#include "helpers.h"
#include "ResourceDescriptors.h"
#include "Logger.h"

// TODO: Smooth Window Resize Implementation
// Steps needed:
// 1. Add cache image/memory/view to store last rendered frame
// 2. Create cache image matching window size (transfer + color attachment usage)
// 3. Modify render process:
//    - Normal: render to cache -> blit to swapchain
//    - During resize: only blit cached image with scaling
// 4. Add cleanup for cache resources
// 5. Consider: memory management, synchronization, layout transitions

using namespace std;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr uint64_t FenceTimeout = 1000000000;
const std::string MODEL_PATH = "models/test_skull.obj";
const std::string TEXTURE_PATH = "textures/test_skull.jpg";
//const std::string TEXTURE_PATH = "textures/heart_texture.png";
class AnubisEngine
{
public:
    void run();

private:
    // window/render functions
    void initWindow();
    void createSurface();
    void createSwapChain(const vk::raii::SwapchainKHR& previousSwapChain);
    void recreateSwapChain();
    void cleanupSwapChain(bool clearSwapChain);
    void createSwapChainImageViews();
    void createMsaaResources();
    void createDepthResources();

    // TODO: these might need to be abstracted to an image library class
    void createTextureImage();
    void createTextureImageView();
    void createTextureImageSampler();
    //
    
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void loadModel();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    // TODO:  it is actually possible to bind multiple descriptor sets simultaneously.
    //  You need to specify a descriptor set layout for each descriptor set when creating the pipeline layout.
    //  You can use this feature to put descriptors that vary per-object and descriptors that are shared into separate descriptor sets.
    void createDescriptorSets();
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const;
    void initSurfaceCapabilities();
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat();
    vk::PresentModeKHR chooseSwapPresentMode();
    vk::Extent2D chooseSwapExtent();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    // command functions
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(uint32_t imageIndex);
    void transitionEngineImageLayoutIndex(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlagBits2 srcAccessMask,
        vk::AccessFlagBits2 dstAccessMask, vk::PipelineStageFlagBits2 srcStageMask, vk::PipelineStageFlagBits2 dstStageMask);
    void transitionEngineImageLayoutImage(vk::raii::Image& image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::AccessFlags2 src_access_mask,
        vk::AccessFlags2 dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask,
        vk::ImageAspectFlags aspect_mask);

    // shaders
    void compileShader(std::string filename, std::string target, std::string profile, std::string vertEntry, std::string fragEntry, std::string outputName);
    
    // vulkan functions
    void initVulkan();
    std::pair<const uint32_t, const char**> getRequiredExtensions();
    std::pair<const uint32_t, const char**> checkGLFWExtensions();
    void createInstance();

    // Vulkan SDK/Config/vk_layer_settings.txt has more info for setting up more layers
    void checkVulkanLayers();
    void setupDebugMessenger();
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // device management
    void pickPhysicalDevice();
    uint32_t findQueueIndex(vk::PhysicalDevice device, const vk::QueueFlagBits flag);
    uint32_t findPresentQueueIndex(vk::PhysicalDevice device, uint32_t& graphicsQueueIndex);
    void createLogicalDevice();

    // main execution functions
    void mainLoop();
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void cleanUpBuffers();
    void cleanup();

    // synchronization functions
    void createSyncObjects();
    
private:
    // window/render members
    GLFWwindow* mainWindow;
    vk::raii::SurfaceKHR mainWindowSurface = nullptr;
    vk::SurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<vk::SurfaceFormatKHR> surfaceFormats;
    std::vector<vk::PresentModeKHR> presentModes;
    vk::SurfaceFormatKHR swapChainImageFormat;
    vk::Extent2D swapChainExtent;
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::raii::ImageView> swapChainImageViews;
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;

    // TODO: Driver developers recommend that multiple buffers should be stored in a single buffer
    //  oh. just like vertex and index buffers. investigate this improvement
    //  reason: data that is sent is more 'cache friendly'
    //      reuse of same memory chunks for multiple resources (if not used for the same render operations)
    //      and provided that their data is refreshed (instancing??)
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory indexBufferMemory = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;

    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;

    vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;
    
    //synchronization members - semaphores and fences
    //usage: semaphore GPU ops
    //       fences CPU ops
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderCompleteSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;
    bool framebufferResized = false;

    // command buffer
    vk::raii::CommandPool commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    uint32_t semaphoreIndex = 0;
    uint32_t currentFrame = 0;

    // vulkan members
    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    //validation layers
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    const std::vector<const char*> validationLayers =
    {
        "VK_LAYER_KHRONOS_validation"
    };
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // device management
    std::vector<vk::raii::PhysicalDevice> physicalDevices;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    std::vector<const char*> requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };
    vk::raii::Device logicalDevice = nullptr;
    float graphicsQueuePriority = 0.0f;
    vk::raii::Queue graphicsQueue = nullptr;
    uint32_t graphicsQueueIndex = 0;
    vk::raii::Queue presentQueue = nullptr;
    uint32_t presentQueueIndex = 0;

    // testing shape
    std::tuple<std::vector<Vertex>, std::vector<uint32_t>> currentShape;// = GeneratedShapes::getDualRectangle();

    // TODO: the below will need to be abstracted a bit for any texture loading
    //  Image Library??
    uint32_t mipLevels;
    vk::raii::Image textureImage = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;
    vk::raii::ImageView textureImageView = nullptr;
    vk::raii::Sampler textureImageSampler = nullptr;

    // TODO: dynamic render targets??
    vk::raii::Image msaaRenderTargetImage = nullptr;
    vk::raii::DeviceMemory msaaRenderTargetImageMemory = nullptr;
    vk::raii::ImageView msaaRenderTargetImageView = nullptr;
};