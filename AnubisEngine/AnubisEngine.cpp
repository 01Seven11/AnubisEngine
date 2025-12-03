#include "AnubisEngine.h"

#include <iostream>
#include <ostream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void AnubisEngine::run()
{
    Logger::init();
#ifdef _DEBUG
    Logger::printToConsole("DEBUG BUILD:", level::debug);
#else
    Logger::printToConsole("RELEASE BUILD:", level::info);
#endif

    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void AnubisEngine::initVulkan()
{
    //compile the test shader
    // shader.slang
    // spirv
    // spriv_1_4
    // vertMain
    // fragMain
    // shader.spv
    //compileShader("shader.slang", "spirv", "spirv_1_4", "vertMain", "fragMain", "shader.spv");
    // compile_shader.bat "shader.slang" "spirv" "spirv_1_4" "vertMain" "fragMain" "shader.spv"
    
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    initSurfaceCapabilities();
    createLogicalDevice();
    createSwapChain(nullptr);
    createSwapChainImageViews();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createMsaaResources();
    createDepthResources();
    createTextureImage();
    createTextureImageView();
    createTextureImageSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void AnubisEngine::createInstance()
{
    // create the application information
    constexpr vk::ApplicationInfo appInfo
    {
        .pApplicationName = "Anubis Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Anubis Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion13
    };

    checkVulkanLayers();

    auto glfwExtensions = checkGLFWExtensions();

    // create the instance information with the appInfo.
    vk::InstanceCreateInfo createInstanceInfo
    {
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = glfwExtensions.first,
        .ppEnabledExtensionNames = glfwExtensions.second
    };

    Logger::printToConsole("***** Creating Vulkan Instance *****");
    instance = vk::raii::Instance(context, createInstanceInfo);
}

void AnubisEngine::setupDebugMessenger()
{
    if (!enableValidationLayers) return;
    Logger::printToConsole("***** Setting up Debug Messenger *****");
    vk::DebugUtilsMessengerCreateInfoEXT createInfo
    {
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallback
    };

    debugMessenger = instance.createDebugUtilsMessengerEXT(createInfo);
}

vk::Bool32 AnubisEngine::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                       vk::DebugUtilsMessageTypeFlagsEXT messageType,
                                       const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                       void* pUserData)
{

    level::level_enum logLevel = level::info;
    switch (messageSeverity)
    {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        logLevel = level::trace;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        logLevel = level::info;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        logLevel = level::warn;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        logLevel = level::err;
        break;
    }
    Logger::printToConsole("validation layer: type " + to_string(messageType) + " msg: " + string(pCallbackData->pMessage), logLevel);
    return vk::False;
}

void AnubisEngine::pickPhysicalDevice()
{
    Logger::printToConsole("***** Picking Physical Device *****");
    Logger::printToConsole("Using Required Device Extensions:");
    for (const auto& extension : requiredDeviceExtensions)
    {
        Logger::printToConsole(extension, level::info);
    }

    physicalDevices = instance.enumeratePhysicalDevices();
    const auto deviceIter = std::ranges::find_if(physicalDevices,
    [&](const auto& device)
    {
        std::string deviceName = device.getProperties().deviceName;

        // vulkan 1.3 must be supported
        bool supportsVulkan1_3 = device.getProperties().apiVersion >=
            VK_API_VERSION_1_3;
        Logger::printToConsole(deviceName + " supports Vulkan 1.3: " + std::to_string(
            supportsVulkan1_3), level::info);

        // ensure that the queue families support graphics
        auto queueFamilies = device.getQueueFamilyProperties();
        bool supportsGraphics =
            std::ranges::any_of(queueFamilies, [](auto const& qfp)
            {
                return !!(qfp.queueFlags &
                    vk::QueueFlagBits::eGraphics);
            });
        Logger::printToConsole(deviceName + " supports graphics: " + std::to_string(
            supportsGraphics), level::info);

        // ensure that the device supports all required extensions
        auto availableDeviceExtensions = device.
            enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
            std::ranges::all_of(requiredDeviceExtensions,
            [&availableDeviceExtensions](auto const& requiredDeviceExtension)
            {
                return std::ranges::any_of(availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension)
                {
                    return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                });
            });
        Logger::printToConsole(deviceName + " supports all required extensions: " + std::to_string(supportsAllRequiredExtensions), level::info);

        // ensure that all required features are availabel
        auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering && 
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState &&
                                        features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy;
        
        Logger::printToConsole(deviceName + " supports all required features: " + std::to_string(supportsRequiredFeatures), level::info);

        bool deviceIsSuitable = supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;

        if (deviceIsSuitable)
        {
            Logger::printToConsole("Found suitable device: " + deviceName, level::info);
            physicalDevice = device;
            msaaSamples = helpers::getMaxUsableSampleCount(physicalDevice);
        }
        else
        {
            Logger::printToConsole("Device not suitable: " + deviceName, level::warn);
        }
        return deviceIsSuitable;
    }
    );

    //TODO: add check if device was actually found!!

    Logger::printToConsole("*************************");
}

uint32_t AnubisEngine::findQueueIndex(vk::PhysicalDevice device, const vk::QueueFlagBits flag)
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();

    // get the first index into queueFamilyProperties which supports the provided QueueFlagBits
    auto graphicsQueueFamilyProperty =
        std::find_if(queueFamilyProperties.begin(),
                     queueFamilyProperties.end(),
                     [&](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & flag; });

    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
}

uint32_t AnubisEngine::findPresentQueueIndex(vk::PhysicalDevice device, uint32_t& graphicsQueueIndex)
{
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = device.getQueueFamilyProperties();
    // determine a queueFamilyIndex that supports present
    // first check if the graphicsIndex is good enough
    auto presentIndex = device.getSurfaceSupportKHR(graphicsQueueIndex, *mainWindowSurface)
                            ? graphicsQueueIndex
                            : ~0;

    if (presentIndex == queueFamilyProperties.size())
    {
        // the graphicsIndex doesn't support present -> look for another family index that supports both
        // graphics and present
        for (size_t i = 0; i < queueFamilyProperties.size(); i++)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *mainWindowSurface))
            {
                graphicsQueueIndex = static_cast<uint32_t>(i);
                presentIndex = graphicsQueueIndex;
                break;
            }
        }
        if (presentIndex == queueFamilyProperties.size())
        {
            // there's nothing like a single family index that supports both graphics and present -> look for another
            // family index that supports present
            for (size_t i = 0; i < queueFamilyProperties.size(); i++)
            {
                if (device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *mainWindowSurface))
                {
                    presentIndex = static_cast<uint32_t>(i);
                    break;
                }
            }
        }
    }
    if ((graphicsQueueIndex == queueFamilyProperties.size()) || (presentIndex == queueFamilyProperties.size()))
    {
        Logger::printToConsole("Could not find a queue for graphics or present -> terminating", level::err);
        throw std::runtime_error("Could not find a queue for graphics or present -> terminating");
    }

    return presentIndex;
}

void AnubisEngine::createLogicalDevice()
{
    // create the logical device
    Logger::printToConsole("***** Creating Logical Device/Queues *****");

    // get the qraphics queue index
    graphicsQueueIndex = findQueueIndex(physicalDevice, vk::QueueFlagBits::eGraphics);
    Logger::printToConsole("Graphics Queue Index: " + std::to_string(graphicsQueueIndex), level::info);

    presentQueueIndex = findPresentQueueIndex(physicalDevice, graphicsQueueIndex);
    Logger::printToConsole("Present Queue Index: " + std::to_string(presentQueueIndex), level::info);
    Logger::printToConsole("Graphics Queue Index (After findPresentQueueIndex): " + std::to_string(graphicsQueueIndex), level::info);

    // setup the device queue info struct for the graphics queue
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo
    {
        .queueFamilyIndex = graphicsQueueIndex,
        .queueCount = 1,
        .pQueuePriorities = &graphicsQueuePriority
    };

    // to be used later
    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = vk::True;
    deviceFeatures.sampleRateShading = vk::True;

    // IMPORTANT: structure chaining!! 'automatically' links the pNext pointer for all the defined types
    // only need to pass the first to the DeviceCreateInfo struct
    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain
    {
        {.features = deviceFeatures},
        {.synchronization2 = true, .dynamicRendering = true},
        {.extendedDynamicState = true}
    };

    // setup the DeviceCreateInfo struct
    // IMPORTANT: this gets executed with all features in the featureChain
    vk::DeviceCreateInfo deviceCreateInfo
    {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
    };

    logicalDevice = vk::raii::Device(physicalDevice, deviceCreateInfo);
    graphicsQueue = vk::raii::Queue(logicalDevice, graphicsQueueIndex, 0);
    presentQueue = vk::raii::Queue(logicalDevice, presentQueueIndex, 0);
    Logger::printToConsole("*************************");
}

std::pair<const uint32_t, const char**> AnubisEngine::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> validationExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers)
    {
        validationExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        glfwExtensionCount++;
    }

    const char** validationExtensionArray = new const char*[validationExtensions.size()];
    std::copy(validationExtensions.begin(), validationExtensions.end(), validationExtensionArray);

    return std::make_pair(glfwExtensionCount, validationExtensionArray);
}

std::pair<const uint32_t, const char**> AnubisEngine::checkGLFWExtensions()
{
    // get the instance extensions from glfw
    auto glfwExtensions = getRequiredExtensions();
    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    // check if the glfw extensions are supported by vulkan
    for (uint32_t i = 0; i < glfwExtensions.first; i++)
    {
        if (std::ranges::none_of(extensionProperties,
                                 [glfwExtension = glfwExtensions.second[i]](const auto& extensionProperty)
                                 {
                                     return strcmp(glfwExtension, extensionProperty.extensionName) == 0;
                                 }))
        {
            Logger::printToConsole("GLFW extension not supported by vulkan: " + std::string(glfwExtensions.second[i]), level::err);
            throw std::runtime_error("GLFW extension not supported by vulkan: " + std::string(glfwExtensions.second[i]));
        }
    }

    return glfwExtensions;
}

void AnubisEngine::checkVulkanLayers()
{
    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(validationLayers, [&layerProperties](auto const& validationLayer)
    {
        return std::ranges::none_of(layerProperties, [validationLayer](auto const& layerProperty)
        {
            return strcmp(validationLayer, layerProperty.layerName) == 0;
        });
    }))
    {
        Logger::printToConsole("One or more required layers are not supported!", level::err);
        throw std::runtime_error("One or more required layers are not supported!");
    }
}

void AnubisEngine::mainLoop()
{
    while (!glfwWindowShouldClose(mainWindow))
    {
        glfwPollEvents();
        drawFrame();
    }

    logicalDevice.waitIdle();
}

void AnubisEngine::drawFrame()
{
    // all are asynchronous
    // 1) wait for the previous frame
    while (vk::Result::eTimeout == logicalDevice.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX));

    // 2) acquire image from the swap chain
    auto [result, imageIndex] = swapChain.acquireNextImage(
        UINT64_MAX, presentCompleteSemaphores[semaphoreIndex], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR ||
        (result == vk::Result::eSuboptimalKHR && framebufferResized))
    {
        recreateSwapChain();
        return;
    }

    // if (result != vk::Result::eSuccess)
    // {
    //     Logger::printToConsole("failed to acquire swap chain image!");
    //     throw std::runtime_error("failed to acquire swap chain image!");
    // }

    // 2a) update currentFrame with the uniformBuffer
    updateUniformBuffer(currentFrame);

    // 3) record a command buffer which draws the scene onto the image
    logicalDevice.resetFences(*inFlightFences[currentFrame]);
    commandBuffers[currentFrame].reset();
    recordCommandBuffer(imageIndex);

    // 4) submit the recorded command buffer
    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo
    {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &(*presentCompleteSemaphores[semaphoreIndex]), // wait for present
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &(*commandBuffers[currentFrame]), // the command buffer to submit
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &(*renderCompleteSemaphores[semaphoreIndex]) // signal complete
    };
    graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

    // 5) present the swap chain image
    const vk::PresentInfoKHR presentInfo
    {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &(*renderCompleteSemaphores[semaphoreIndex]), // wait for render
        .swapchainCount = 1,
        .pSwapchains = &(*swapChain), // the swap chain to present
        .pImageIndices = &imageIndex, // the index of the swap chain image to present
        .pResults = nullptr // not used
    };

    try
    {
        result = presentQueue.presentKHR(presentInfo);
        if (result == vk::Result::eErrorOutOfDateKHR ||
            (framebufferResized && result == vk::Result::eSuboptimalKHR))
        {
            framebufferResized = false;
            recreateSwapChain();
        }
    }
    catch (vk::OutOfDateKHRError&)
    {
        result = vk::Result::eErrorOutOfDateKHR;
    }

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != vk::Result::eSuccess)
    {
        Logger::printToConsole("failed to present swap chain image!");
        throw std::runtime_error("failed to present swap chain image!");
    }

    semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphores.size();
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void AnubisEngine::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    // identity, rotation angle, and rotation axis
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // eye position, center position, up axis
    ubo.view = glm::lookAt(glm::vec3(0.0f, 12.0f, 60.0f), glm::vec3(0.0f, 12.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    // fov, aspect ratio, near clip, far clip
    ubo.proj = glm::perspective(glm::radians(35.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.01f, 75.0f);

    // glm is OpenGL. the Y coord of the clip coordinates is inverted.
    // flip the sign on the scaling factor of the y-axis in the proj.
    // if not done, the image will appear upside down
    ubo.proj[1][1] *= -1;

    //write directly out!!
    // still not he most efficent see 'push constants'
    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void AnubisEngine::cleanUpBuffers()
{
    Logger::printToConsole("***** Cleaning Up Uniform Buffers Memory *****");
    for (auto& buffer : uniformBuffersMemory)
    {
        buffer.clear();
        buffer = nullptr;
    }
    
    Logger::printToConsole("***** Cleaning Up Uniform Buffers *****");
    for (auto& buffer : uniformBuffers)
    {
        buffer.clear();
        buffer = nullptr;
    }
    
    Logger::printToConsole("Cleaning up Index Buffer Memory");
    indexBufferMemory.clear();
    indexBuffer = nullptr;
    
    Logger::printToConsole("Cleaning up Index Buffer");
    indexBuffer.clear();
    indexBuffer = nullptr;
    
    Logger::printToConsole("Cleaning up Vertex Buffer Memory");
    vertexBufferMemory.clear();
    vertexBufferMemory = nullptr;
    
    Logger::printToConsole("Cleaning up Vertex Buffer");
    vertexBuffer.clear();
    vertexBuffer = nullptr;
}

void AnubisEngine::cleanup()
{
    Logger::printToConsole("***** Cleaning up *****");

    Logger::printToConsole("Cleaning Up Render Target Image Memory");
    msaaRenderTargetImageMemory.clear();
    msaaRenderTargetImageMemory = nullptr;

    Logger::printToConsole("Cleaning Up Render Target Image View");
    msaaRenderTargetImageView.clear();
    msaaRenderTargetImageView = nullptr;

    Logger::printToConsole("Cleaning Up Render Target Image");
    msaaRenderTargetImage.clear();
    msaaRenderTargetImage = nullptr;
    
    Logger::printToConsole("Cleaning Up Texture Image Memory");
    textureImageMemory.clear();
    textureImageMemory = nullptr;

    Logger::printToConsole("Cleaning Up Texture Image View");
    textureImageView.clear();
    textureImageView = nullptr;

    Logger::printToConsole("Cleaning Up Texture Image");
    textureImage.clear();
    textureImage = nullptr;

    Logger::printToConsole("Cleaning up Depth Image Memory");
    depthImageMemory.clear();
    depthImageMemory = nullptr;

    Logger::printToConsole("Cleaning Up Depth Image View");
    depthImageView.clear();
    depthImageView = nullptr;
    
    Logger::printToConsole("Cleaning up Depth Image");
    depthImage.clear();
    depthImage = nullptr;
    
    Logger::printToConsole("Cleaning Up Descriptor Sets");
    for (auto& descriptorSet : descriptorSets)
    {
        descriptorSet.clear();
        descriptorSet = nullptr;
    }
    
    Logger::printToConsole("Cleaning Up Descriptor Pool");
    descriptorPool.clear();
    descriptorPool = nullptr;

    cleanUpBuffers();

    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Semaphores/Fences");
    presentCompleteSemaphores.clear();
    renderCompleteSemaphores.clear();
    inFlightFences.clear();

    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Command Buffer.");
    commandBuffers.clear();
    
    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Command Pool.");
    commandPool.clear();
    
    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Graphics Pipeline.");
    graphicsPipeline.clear();

    Logger::printToConsole("Clearing Descriptor Set Layout.");
    descriptorSetLayout.clear();
    descriptorSetLayout = nullptr;
    
    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Pipeline Layout.");
    pipelineLayout.clear();
    
    cleanupSwapChain(true);

    // not having this here prevents the destruction of < VkSurfaceKHR >
    Logger::printToConsole("Clearing mainWindowSurface.");
    mainWindowSurface.clear();
    mainWindowSurface = nullptr;

    Logger::printToConsole("Destroying window.");
    glfwDestroyWindow(mainWindow);

    Logger::printToConsole("Terminating GLFW.");
    glfwTerminate();

    Logger::printToConsole("*************************");
}

void AnubisEngine::createSyncObjects()
{
    Logger::printToConsole("***** Creating Sync Objects *****");
    presentCompleteSemaphores.clear();
    renderCompleteSemaphores.clear();
    inFlightFences.clear();

    for (int i = 0; i < swapChainImages.size(); i++)
    {
        presentCompleteSemaphores.emplace_back(vk::raii::Semaphore(logicalDevice, vk::SemaphoreCreateInfo()));
        renderCompleteSemaphores.emplace_back(vk::raii::Semaphore(logicalDevice, vk::SemaphoreCreateInfo()));
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        inFlightFences.emplace_back(vk::raii::Fence(logicalDevice, {.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
    Logger::printToConsole("*************************");
}

void AnubisEngine::initWindow()
{
    // initialize the library
    Logger::printToConsole("***** Initializing GLFW *****");
    glfwInit();

    // create a windowed mode window and its OpenGL context (no api)
    Logger::printToConsole("Setting glfwWindowHint GLFW_CLIENT_API|GLFW_NO_API.");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // disable re-sizing for meow
    Logger::printToConsole("Setting glfwWindowHint GLFW_RESIZABLE|GLFW_FALSE.");
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // monitor = display monitor
    // share = OpenGL only
    Logger::printToConsole("Creating Window...");
    mainWindow = glfwCreateWindow(WIDTH, HEIGHT, "Anubis Engine", nullptr, nullptr);

    //store pointer to the instance of this engine
    glfwSetWindowUserPointer(mainWindow, this);
    //register the callback
    glfwSetFramebufferSizeCallback(mainWindow, framebufferResizeCallback);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createSurface()
{
    Logger::printToConsole("***** Creating Surface *****");
    VkSurfaceKHR windowSurface;
    if (glfwCreateWindowSurface(*instance, mainWindow, nullptr, &windowSurface) != 0)
    {
        Logger::printToConsole("Failed to create window surface!", level::err);
        throw std::runtime_error("Failed to create window surface!");
    }

    mainWindowSurface = vk::raii::SurfaceKHR(instance, windowSurface);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createSwapChain(const vk::raii::SwapchainKHR& previousSwapChain)
{
    Logger::printToConsole("***** Creating Swap Chain *****");
    swapChainImageFormat = chooseSwapSurfaceFormat();
    swapChainExtent = chooseSwapExtent();

    // the + 1 here is to increase the performance of the swap chain on 'refresh'
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount + 1);
    Logger::printToConsole("Min Image Count: " + std::to_string(minImageCount), level::info);

    // check against max
    // 0 for surfaceCapabilities.maxImageCount means that there is no limit
    minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                        ? surfaceCapabilities.maxImageCount
                        : minImageCount;
    Logger::printToConsole("Min Image Count (After maxImageCount check): " + std::to_string(minImageCount), level::info);

    // determine imageSharingMode, queueFamilyIndexCount, and pQueueFamilyIndices
    uint32_t queueFamilyIndices[] = {graphicsQueueIndex, presentQueueIndex};
    vk::SharingMode imageShareMode;
    uint32_t queueFamilyIndexCount = 0;
    if (graphicsQueueIndex != presentQueueIndex)
    {
        // concurrent = can be shared across multiple queues w/o explicit ownership
        imageShareMode = vk::SharingMode::eConcurrent;
        queueFamilyIndexCount = 2;
    }
    else
    {
        // exclusively = only owned by one queue at a time
        imageShareMode = vk::SharingMode::eExclusive;
        queueFamilyIndexCount = 0;
    }
    Logger::printToConsole("Image Share Mode: " + vk::to_string(imageShareMode));
    Logger::printToConsole("Queue Family Index Count: " + std::to_string(queueFamilyIndexCount));

    //create the info struct for the swap chain
    vk::SwapchainCreateInfoKHR swapChainCreateInfo
    {
        .flags = vk::SwapchainCreateFlagsKHR(),
        .surface = mainWindowSurface,
        .minImageCount = minImageCount,
        .imageFormat = swapChainImageFormat.format,
        .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
        .imageExtent = swapChainExtent,
        // specifies the number of layers per image (always 1 unless steroscopic)
        .imageArrayLayers = 1,
        // specifies the operations the images in the swap chain will be used for
        // use VK_IMAGE_USAGE_TRANSFER_DST_BIT for post-processing (requires memory operation to transfer rendered image to swap chain image)
        // at the time of this comment, eColorAttachment can be OR'd (we'll see what happens >))
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = imageShareMode,
        .queueFamilyIndexCount = queueFamilyIndexCount,
        .pQueueFamilyIndices = (queueFamilyIndexCount == 0) ? nullptr : queueFamilyIndices,
        // can adjust image transform overall here??
        .preTransform = surfaceCapabilities.currentTransform,
        // ignore alpha (not sure how this will affect transparent options, but we'll see)
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = chooseSwapPresentMode(),
        // don’t care about the color of pixels that are obscured
        .clipped = true,
        //USED WHEN REBUILDING THE SWAPCHAIN!!
        .oldSwapchain = previousSwapChain
    };
    // when to clean the previous swap chain ?!?

    swapChain = vk::raii::SwapchainKHR(logicalDevice, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
    Logger::printToConsole("*************************");
}

// recreation of the swap chain gets triggered when:
//  1) window size updates
// recreate render pass when:
//  1) moving a window from standard range to high dynamic range monitor
void AnubisEngine::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(mainWindow, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(mainWindow, &width, &height);
        glfwWaitEvents();
    }
    
    Logger::printToConsole("***** Recreating Swap Chain *****");
    // again, don't touch resources until they're free.
    logicalDevice.waitIdle();

    // Update surface capabilities before recreation
    surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*mainWindowSurface);

    vk::raii::SwapchainKHR previousSwapChain = std::move(swapChain);

    // properly free swap chain resources.
    cleanupSwapChain(false);
    commandBuffers.clear();

    // obvs: see func name
    createSwapChain(previousSwapChain);
    
    // ImageViews are based on swapChain
    createSwapChainImageViews();
    createMsaaResources();
    createDepthResources();
    createCommandBuffers();

    Logger::printToConsole("*************************");
}

void AnubisEngine::cleanupSwapChain(bool clearSwapChain)
{
    // not having this here prevents the destruction of < VkDevice >
    Logger::printToConsole("Clearing Image Views.");
    for (auto& imageView : swapChainImageViews)
    {
        imageView.clear();
        imageView = nullptr;
    }
    swapChainImageViews.clear();
    
    if (clearSwapChain)
    {
        // not having this here prevents the destruction of < VkSurfaceKHR >
        Logger::printToConsole("Clearing Swapchain.");
        swapChain.clear();
        swapChain = nullptr;
    }
}

void AnubisEngine::createSwapChainImageViews()
{
    Logger::printToConsole("***** Creating Swap Chain Image Views *****");
    vk::ImageViewCreateInfo imageViewCreateInfo{
        .viewType = vk::ImageViewType::e2D,
        .format = swapChainImageFormat.format,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
    };
    for ( auto image : swapChainImages )
    {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back( logicalDevice, imageViewCreateInfo );
    }
    Logger::printToConsole("*************************");
}

void AnubisEngine::createMsaaResources()
{
    Logger::printToConsole("***** Creating MSAA Resources *****");
    vk::Format msaaFormat = swapChainImageFormat.format;

    helpers::createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, msaaFormat,
        vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment
        , vk::MemoryPropertyFlagBits::eDeviceLocal,
        msaaRenderTargetImage, msaaRenderTargetImageMemory,
        logicalDevice, physicalDevice
        );

    msaaRenderTargetImageView = helpers::createImageView(msaaRenderTargetImage, msaaFormat, 1, vk::ImageAspectFlagBits::eColor, logicalDevice);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createDepthResources()
{
    Logger::printToConsole("***** Creating Depth Resources *****");

    //get the depth format
    vk::Format depthFormat = helpers::findDepthFormat(physicalDevice);

    // create the depth image and image view
    helpers::createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat,
                         vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment
                         ,
                         vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory,
                         logicalDevice, physicalDevice);

    depthImageView = helpers::createImageView(depthImage, depthFormat, 1, vk::ImageAspectFlagBits::eDepth, logicalDevice);
    
    Logger::printToConsole("*************************");
}

void AnubisEngine::createTextureImage()
{
    Logger::printToConsole("***** Creating Texture Image *****");

    // load the image an upload it into a Vulkan image object
    //  this will involve commandBuffers
    int texWidth, texHeight, texChannels;
    //stbi_uc* pixels = stbi_load("textures/heart_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4; // 4 bytes per pixel

    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    Logger::printToConsole("Mip Levels: " + std::to_string(mipLevels), level::info);
    
    if (!pixels)
    {
        Logger::printToConsole("Failed to load texture image!", level::err);
        throw std::runtime_error("Failed to load texture image!");
    }

    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    helpers::createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory,
        logicalDevice, physicalDevice);

    // copy the image pixels to the buffer
    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    vk::Format textureFormat = vk::Format::eR8G8B8A8Srgb;

    helpers::createImage(texWidth, texHeight, mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                         vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                         vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory,
                         logicalDevice, physicalDevice);

    // copy staging buffer to image
    helpers::transitionImageLayoutTexture(textureImage, textureFormat, mipLevels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, commandPool, logicalDevice, graphicsQueue);
    helpers::copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), commandPool, logicalDevice, graphicsQueue);
    // helpers::transitionImageLayoutTexture(textureImage, textureFormat, mipLevels, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, commandPool, logicalDevice, graphicsQueue);
    // the above transition should happen when generating the mip maps
    helpers::generateMipmaps(textureImage, textureFormat, mipLevels, texWidth,texHeight, commandPool, logicalDevice, physicalDevice, graphicsQueue);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createTextureImageView()
{
    Logger::printToConsole("***** Creating Texture Image View *****");

    textureImageView = helpers::createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, mipLevels, vk::ImageAspectFlagBits::eColor, logicalDevice);
    
    Logger::printToConsole("*************************");
}

void AnubisEngine::createTextureImageSampler()
{
    Logger::printToConsole("***** Creating Texture Image Sampler *****");
    vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
    vk::SamplerCreateInfo samplerCreateInfo
    {
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        //uv coords
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE, // vk::False to disable
        .maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy, // limits the number of texel samples. if disabled, this should be 1.0
        .compareEnable = VK_FALSE, // mainly used in percentage-closer filtering
        .compareOp = vk::CompareOp::eAlways,
        // lod control
        // TODO: adjust based on distance
        .minLod = 0.0f,
        .maxLod = static_cast<float>(mipLevels),
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = VK_FALSE, // which coord system to use for uv false = [0, 1) : true = [0, texWidth) and [0, texHeight)
    };

    textureImageSampler = vk::raii::Sampler(logicalDevice, samplerCreateInfo);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createDescriptorSetLayout()
{
    Logger::printToConsole("***** Creating Descriptor Set Layout *****");

    // TODO: it's possible for the shader variable to be an array
    //  add handling for this
    std::array bindings = {
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
        // for image sampling related descriptors
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
        // NOTE: texture sampling for the vertex shader is usually for height-mapping
    };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo
    {
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    descriptorSetLayout = vk::raii::DescriptorSetLayout(logicalDevice, layoutCreateInfo);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createGraphicsPipeline()
{
    Logger::printToConsole("***** Creating Graphics Pipeline *****");

    Logger::printToConsole("Creating Stages:");
    auto shaderCode = helpers::readFile("shaders/shader.spv");
    Logger::printToConsole("Shader Binary Size: " + std::to_string(shaderCode.size()), level::info);

    vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageCreateInfo
    {
        // .stage: F12 that shit
        .stage = vk::ShaderStageFlagBits::eVertex,
        // .module the code to run
        .module = shaderModule,
        // the function to invoke
        // this means that it’s possible to combine multiple fragment shaders into a single shader module and use different entry points to differentiate between their behaviors.
        //  TODO:post-process library??
        .pName = "vertMain",

        // TODO: reseach more into the below
        // pSpecifializationInfo: allows you to specify values for shader constants.
        // You can use a single shader module where its behavior can be configured in pipeline creation by specifying different values for the constants used in it.
        // more efficient than configuring the shader using variables at render time, because the compiler can do optimizations like eliminating if statements that depend on these values.
        .pSpecializationInfo = nullptr
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageCreateInfo
    {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain",
        .pSpecializationInfo = nullptr
    };

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageCreateInfo, fragShaderStageCreateInfo};

    Logger::printToConsole("Creating Vertex Input:");
    
    // pretty sure this is where we're reading/sending vertex data from/to the GPU
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo
    {
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    Logger::printToConsole("Creating Input Assembly:");

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo
    {
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = false
    };

    Logger::printToConsole("Creating Viewport:");
    // viewport is set/defined in recordCommanBuffer
    // TODO: Make viewport and scissor states dynamic (more flexibility w/o performance lose?! fuck yeah.)
    //  update the tutorial musta been reading minds (it provides that option)
    // viewports define the transformation from the image to the framebuffer
    vk::Viewport viewport
    {
        0.0f,
        0.0f,
        static_cast<float>(swapChainExtent.width),
        static_cast<float>(swapChainExtent.height),
        // the below is for the frame buffer
        0.0f,
        1.0f
    };

    // scissor rectangles define in which region pixels will actually be stored
    // cover the swapChainExtent
    // can be specified as a static/dynamic part of the pipeline
    // if dynamic = true set in the command buffer
    vk::Rect2D scissor
    {
        vk::Offset2D(0, 0),
        swapChainExtent
    };

    // static = renders the viewport and scissor fixed to the swapChainExtent
    // static version: vk::PipelineViewportStateCreateInfo viewportStateInfo {.viewportCount = 1, .scissorCount = 1};
    // for dynamic the ACTUAL viewports and scissors will be setup at draw time
    // also useful for multiple viewports on the GPU
    vk::PipelineViewportStateCreateInfo viewportStateInfo
    {
        .viewportCount = 1,
        //.pViewports = &viewport,
        .scissorCount = 1,
        //.pScissors = &scissor
    };

    // create the rasterizer
    Logger::printToConsole("Creating Rasterizer:");
    // colors fragments, depth testing, face culling and scissor test
    // output fragments that fill the entire polys or wireframe
    vk::PipelineRasterizationStateCreateInfo rasterizerInfo
    {
        // depthClampEnable = true allows for shadow map creation
        .depthClampEnable = false,
        // WILL DISABLE OUTPUT TO THE FRAMEBUFFER IF TRUE
        .rasterizerDiscardEnable = false,
        // wires and points
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        //specifies vertex order
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = false,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 1.0f,
        // required for point and wire configurations
        // values > 1.0 must have wideLines enabled GPU feature
        .lineWidth = 1.0f
    };

    // create the multisampling
    // enabling requires enabling a GPU feature
    Logger::printToConsole("Creating Multisampling: [enabled]");
    // antialiasing
    vk::PipelineMultisampleStateCreateInfo multisamplingInfo
    {
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = true,
        .minSampleShading = 0.2f,
        // .pSampleMask = nullptr,
        // .alphaToCoverageEnable = false,
        // .alphaToOneEnable = false
    };

    // create depth
    Logger::printToConsole("Creating Depth: [enabled]");
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo
    {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False
    };

    // create color blending
    Logger::printToConsole("Creating Color Blending: [enabled]");
    // two ways to color blend
    //  1) mix old and new to produce final
    //  2) combine using bitops

    // per attached framebuffer color blending
    // remember to enable for multiple framebuffers
    vk::PipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable = false, // new color from the fragment shader is passed through unmodified
        // .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        // .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        // .colorBlendOp = vk::BlendOp::eAdd,
        // .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        // .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        // .alphaBlendOp = vk::BlendOp::eAdd,
        // determine which channels are actually passed through
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };
    // global color blending
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo
    {
        .logicOpEnable = false,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // TODO: implement alpha blending (blend colors based on their opacity during compute)
    //  finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
    //  finalColor.a = newAlpha.a;

    Logger::printToConsole("Creating Dynamic States:");
    // These are the things that can change before triggering recreation of the pipeline
    // setting these causes the configuration of these values to be ignored, and I should be able to
    // supply the settings at draw time. Kinda neat.
    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo
    {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    Logger::printToConsole("Creating Pipeline Layout:");

    // to specify uniform shader values, they are created throughthe piplineLayoutCreateInfo
    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo
    {
        .setLayoutCount = 1,
        .pSetLayouts = &*descriptorSetLayout,
        // dynamic values == pushConstant
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    pipelineLayout = vk::raii::PipelineLayout(logicalDevice, pipelineLayoutCreateInfo);

    vk::Format depthFormat = helpers::findDepthFormat(physicalDevice);
    // using one color attachment with the format of our swap chain
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo
    {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainImageFormat.format,
        .depthAttachmentFormat = depthFormat
    };

    // create the pipeline
    Logger::printToConsole("Creating Graphics Pipeline:");
    vk::GraphicsPipelineCreateInfo pipelineCreateInfo
    {
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pTessellationState = nullptr,
        .pViewportState = &viewportStateInfo,
        .pRasterizationState = &rasterizerInfo,
        .pMultisampleState = &multisamplingInfo,
        .pDepthStencilState = &depthStencilInfo,
        .pColorBlendState = &colorBlendInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = pipelineLayout,
        // dynamic rendering allows us to change attachments without creating new render pass objects
        // eliminate render pass and framebuffer objects
        // i.e., can refer directly to the image view w/o a framebuffer
        .renderPass = nullptr,
        .basePipelineHandle = nullptr,
        // pipeline inheritance
        // can only be used if VK_PIPELINE_CREATE_DERIVATIVE_BIT is set in the flags field
        .basePipelineIndex = -1
    };

    // capable of creating multiple pipelines in a single call
    // pipelineCache
    graphicsPipeline = vk::raii::Pipeline(logicalDevice, nullptr, pipelineCreateInfo);
}

// TODO: move this to a class that can support entities
void AnubisEngine::loadModel()
{
    Logger::printToConsole("***** Loading Test Model *****");
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
    {
        Logger::printToConsole("Failed to load model!", level::err);
        throw std::runtime_error("Failed to load model!");
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    vector<Vertex> vertices;
    vector<uint32_t> indices;
    //combine all of the faces into a single model
    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.uv =
            {
                attrib.texcoords[2 * index.texcoord_index + 0],
                // The OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image.
                // however, we’ve uploaded our image into Vulkan in a top-to-bottom orientation where 0 means the top of the image.
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0, 1.0, 1.0};

            if (!uniqueVertices.contains(vertex))
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    currentShape = make_tuple(vertices, indices);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createVertexBuffer()
{
    Logger::printToConsole("***** Creating Vertex Buffer *****");
    const std::vector<Vertex> vertices = std::get<0>(currentShape);
    vk::DeviceSize bufferSize = vertices.size() * sizeof(Vertex);
    
    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    helpers::createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory,
        logicalDevice, physicalDevice);

    // map the buffer memory into CPU accessible memory
    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    // this may not be an immediate transfer
    memcpy(data, vertices.data(), (size_t) bufferSize);
    stagingBufferMemory.unmapMemory();
    
    helpers::createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vertexBuffer, vertexBufferMemory,
        logicalDevice, physicalDevice);

    helpers::copyBuffer(stagingBuffer, vertexBuffer, bufferSize, commandPool, logicalDevice, graphicsQueue);
    
    Logger::printToConsole("*************************");
}

void AnubisEngine::createIndexBuffer()
{
    Logger::printToConsole("***** Creating Index Buffer *****");
    const std::vector<uint32_t> indices = std::get<1>(currentShape);
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
    vk::raii::Buffer stagingBuffer({});
    vk::raii::DeviceMemory stagingBufferMemory({});
    helpers::createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer, stagingBufferMemory,
        logicalDevice, physicalDevice);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), (size_t) bufferSize);
    stagingBufferMemory.unmapMemory();
    
    helpers::createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        indexBuffer, indexBufferMemory,
        logicalDevice, physicalDevice
    );

    helpers::copyBuffer(stagingBuffer, indexBuffer, bufferSize, commandPool, logicalDevice, graphicsQueue);
    
    Logger::printToConsole("*************************");
}

// 'persistent mapping' - The buffer stays mapped to this pointer for the application’s whole lifetime.
//  Not having to map the buffer every time we need to update, it increases performance.
void AnubisEngine::createUniformBuffers()
{
    Logger::printToConsole("***** Creating Uniform Buffers *****");
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        // create the buffer
        vk::raii::Buffer uniformBuffer({});
        vk::raii::DeviceMemory uniformBufferMemory({});
        helpers::createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            uniformBuffer, uniformBufferMemory,
            logicalDevice, physicalDevice);

        uniformBuffers.emplace_back(std::move(uniformBuffer));
        uniformBuffersMemory.emplace_back(std::move(uniformBufferMemory));
        uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
    }
    Logger::printToConsole("*************************");
}
// Descriptor sets can’t be created directly, they must be allocated from a pool like command buffers.
void AnubisEngine::createDescriptorPool()
{
    Logger::printToConsole("***** Creating Descriptor Pool *****");
    // describe which descriptor types our descriptor sets are going to contain and how many of them
    
    std::array poolSize = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
    };
    //allocate one each frame
    vk::DescriptorPoolCreateInfo descriptorPoolInfo
    {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
        .pPoolSizes = poolSize.data()
    };

    descriptorPool = vk::raii::DescriptorPool(logicalDevice, descriptorPoolInfo);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createDescriptorSets()
{
    Logger::printToConsole("***** Creating Descriptor Sets *****");
    // specify the descriptor pool to allocate from, the number to allocate and the layout to base them on
    // create one for each frame in flight
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
    vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo
    {
        .descriptorPool = descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()
    };
    
    descriptorSets = logicalDevice.allocateDescriptorSets(descriptorSetAllocateInfo);

    //configure the individual descriptorSets
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo
        {
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        vk::DescriptorImageInfo imageInfo
        {
            .sampler = textureImageSampler,
            .imageView = textureImageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };

        std::array descriptorWrites = {
            vk::WriteDescriptorSet {
                        .dstSet = descriptorSets[i],
                        .dstBinding = 0, //index in the array we want to update
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                        .pBufferInfo = &bufferInfo
                    },
            vk::WriteDescriptorSet {
                        .dstSet = descriptorSets[i],
                        .dstBinding = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                        .pImageInfo = &imageInfo
            }
        };
        

        logicalDevice.updateDescriptorSets(descriptorWrites, {});
    }
    
    Logger::printToConsole("*************************");
}

[[nodiscard]]vk::raii::ShaderModule AnubisEngine::createShaderModule(const std::vector<char>& code) const
{
    Logger::printToConsole("***** Creating Shader Module *****");
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo
    {
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    vk::raii::ShaderModule shaderModule(logicalDevice, shaderModuleCreateInfo);
    Logger::printToConsole("*************************");
    return shaderModule;
}

void AnubisEngine::initSurfaceCapabilities()
{
    Logger::printToConsole("***** Initializing Surface Capabilities *****");

    // get the capabilities
    surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(mainWindowSurface);
    Logger::printToConsole("Surface Capabilities:");
    Logger::printToConsole("Current Transform: " + vk::to_string(surfaceCapabilities.currentTransform), level::info);
    Logger::printToConsole("Current Width: " + std::to_string(surfaceCapabilities.currentExtent.width), level::info);
    Logger::printToConsole("Current Height: " + std::to_string(surfaceCapabilities.currentExtent.height), level::info);
    Logger::printToConsole("Min Image Count: " + std::to_string(surfaceCapabilities.minImageCount), level::info);
    Logger::printToConsole("Max Image Count: " + std::to_string(surfaceCapabilities.maxImageCount), level::info);
    Logger::printToConsole("Min Image Extent Width: " + std::to_string(surfaceCapabilities.minImageExtent.width), level::info);
    Logger::printToConsole("Min Image Extent Height: " + std::to_string(surfaceCapabilities.minImageExtent.height), level::info);
    Logger::printToConsole("Max Image Extent Width: " + std::to_string(surfaceCapabilities.maxImageExtent.width), level::info);
    Logger::printToConsole("Max Image Extent Height: " + std::to_string(surfaceCapabilities.maxImageExtent.height), level::info);
    Logger::printToConsole("Supported Composite Alpha: " + vk::to_string(surfaceCapabilities.supportedCompositeAlpha), level::info);
    Logger::printToConsole("Supported Transforms: " + vk::to_string(surfaceCapabilities.supportedTransforms), level::info);
    Logger::printToConsole("Supported Usage Flags: " + vk::to_string(surfaceCapabilities.supportedUsageFlags), level::info);
    Logger::printToConsole("Surface Max Image Array Layers: " + std::to_string(surfaceCapabilities.maxImageArrayLayers), level::info);

    // get the formats
    surfaceFormats = physicalDevice.getSurfaceFormatsKHR(mainWindowSurface);
    Logger::printToConsole("Surface Formats:");
    for (auto& format : surfaceFormats)
    {
        Logger::printToConsole(vk::to_string(format.format) + " " + vk::to_string(format.colorSpace), level::info);
    }

    // get the present modes
    presentModes = physicalDevice.getSurfacePresentModesKHR(mainWindowSurface);
    Logger::printToConsole("Surface Present Modes:");
    for (auto& mode : presentModes)
    {
        Logger::printToConsole(vk::to_string(mode), level::info);
    }
    Logger::printToConsole("*************************");
}

vk::SurfaceFormatKHR AnubisEngine::chooseSwapSurfaceFormat()
{
    Logger::printToConsole("***** Choosing Swap Surface Format *****");
    auto selectedSurfaceFormat = std::ranges::find_if(surfaceFormats, [](auto const& format)
                                                      {
                                                          return format.format == vk::Format::eB8G8R8A8Srgb && format.
                                                              colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                                                      }
    );

    if (selectedSurfaceFormat != surfaceFormats.end())
    {
        Logger::printToConsole("Found eB8G8R8A8Srgb:eSrgbNonlinear surface format.", level::info);
        Logger::printToConsole("*************************");
        return *selectedSurfaceFormat;
    }
    else
    {
        Logger::printToConsole(
            "Did not find eB8G8R8A8Srgb:eSrgbNonlinear surface format. Using first available format.");
        Logger::printToConsole("*************************");
        return surfaceFormats[0];
    }
}

vk::PresentModeKHR AnubisEngine::chooseSwapPresentMode()
{
    Logger::printToConsole("***** Choosing Swap Present Mode *****");
    auto selectedPresentMode = std::ranges::find_if(presentModes, [](auto const& mode)
                                                    {
                                                        return mode == vk::PresentModeKHR::eMailbox;
                                                    }
    );
    if (selectedPresentMode != presentModes.end())
    {
        Logger::printToConsole("Found eMailbox present mode.", level::info);
        Logger::printToConsole("*************************");
        return *selectedPresentMode;
    }
    else
    {
        Logger::printToConsole("Did not find eMailbox present mode. Using FIFO available mode.");
        Logger::printToConsole("*************************");
        // FIFO is guaranteed
        return vk::PresentModeKHR::eFifo;
    }
}

vk::Extent2D AnubisEngine::chooseSwapExtent()
{
    Logger::printToConsole("***** Choosing Swap Extent *****");
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        Logger::printToConsole("Using Current Extent.", level::info);
        return surfaceCapabilities.currentExtent;
    }

    Logger::printToConsole("Using GLFW Extent.", level::info);
    Logger::printToConsole("*************************");
    // query the frame buffer size from glfw
    int width, height;
    // this works on resize. called as part of recreation (window resize event)
    glfwGetFramebufferSize(mainWindow, &width, &height);

    return {
        std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height,
                             surfaceCapabilities.maxImageExtent.height)
    };
}

void AnubisEngine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto thisEngine = static_cast<AnubisEngine*>(glfwGetWindowUserPointer(window));
    thisEngine->framebufferResized = true;
}

void AnubisEngine::createCommandPool()
{
    Logger::printToConsole("***** Creating Command Pool *****");
    vk::CommandPoolCreateInfo commandPoolCreateInfo
    {
        // recorded individually - recording every frame
        // use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT if commands are rerecorded often
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        // for graphics
        .queueFamilyIndex = graphicsQueueIndex
    };

    commandPool = vk::raii::CommandPool(logicalDevice, commandPoolCreateInfo);
    Logger::printToConsole("*************************");
}

void AnubisEngine::createCommandBuffers()
{
    Logger::printToConsole("***** Creating Command Buffer *****");
    commandBuffers.clear();
    // allocate the command buffer
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo
    {
        .commandPool = commandPool,
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY - submittable, but can't be used by other command buffers
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY - can't be submitted directly, but can be called by primary command buffers
        // secondary is useful for reusing common operations
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    commandBuffers = vk::raii::CommandBuffers(logicalDevice, commandBufferAllocateInfo);
    Logger::printToConsole("*************************");
}

void AnubisEngine::recordCommandBuffer(uint32_t imageIndex)
{
    // always begin with ->begin()
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
    // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
    // pInheritanceInfo - It specifies which state to inherit from the calling primary command buffers
    commandBuffers[currentFrame].begin({ });

    // transition the image layout to optimal color attachment
    transitionEngineImageLayoutIndex(imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {}, // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
        vk::PipelineStageFlagBits2::eTopOfPipe, // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput); // dstStage

    //transition the multisampled color image to color attachment
    transitionEngineImageLayoutImage(
        msaaRenderTargetImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {}, // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite, // dstAccessMask
        vk::PipelineStageFlagBits2::eTopOfPipe, // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor
    );

    // transition the depth image to depth attachment optimal layout
    transitionEngineImageLayoutImage(
        depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests,
        vk::ImageAspectFlagBits::eDepth// | vk::ImageAspectFlagBits::eStencil
    );
    
    //set the color attachment
    // clear to black and store the resulting black
    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
    
    vk::RenderingAttachmentInfo attachmentInfo
    {
        //.imageView = swapChainImageViews[imageIndex], // the view to render to
        .imageView = msaaRenderTargetImageView,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal, // layout during rendering
        .resolveMode = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView = swapChainImageViews[imageIndex],
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear, // what to do before rendering
        .storeOp = vk::AttachmentStoreOp::eStore, // after rendering
        .clearValue = clearColor
    };

    vk::RenderingAttachmentInfo depthAttachmentInfo
    {
        .imageView = depthImageView,
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eDontCare,
        .clearValue = clearDepth
    };

    vk::RenderingInfo renderingInfo
    {
        .renderArea = { .offset = {0,0}, .extent = swapChainExtent}, // size of the render area
        .layerCount = 1, // num layers to render to
        .colorAttachmentCount = 1, // expectant layer count
        .pColorAttachments = &attachmentInfo, // the attachment info
        .pDepthAttachment = &depthAttachmentInfo
    };

    commandBuffers[currentFrame].beginRendering(renderingInfo);

    // basic drawing commands

    // bind the graphics pipeline
    commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

    // set up viewport and scissor
    commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
    commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

    // bind the vertex buffer
    commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});

    // bind the index buffer
    // if the index type changes to uint32_t, update this!!
    commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

    // issue the draw command for the triangle,
    // we technically do not have a vert buffer at this point. verts stored in shader (beginning test shader).
    // commandBuffers[currentFrame].draw(3, 1, 0, 0);

    // update the descriptor sets
    // descriptor sets are not unique to any specific pipeline
    //  they can be either graphic or command
    commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr);
    
    // now using indexing
    commandBuffers[currentFrame].drawIndexed(static_cast<uint32_t>(std::get<1>(currentShape).size()), 1, 0, 0, 0);
    
    commandBuffers[currentFrame].endRendering();

    // transition the image for presentation
    transitionEngineImageLayoutIndex(imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe);

    commandBuffers[currentFrame].end();
}

// transition the layout to one that is suitable for rendering
// images can be in different layout that are optimized for different operations
// i.e., optimal for presenting vs. optitmal for color attachment
void AnubisEngine::transitionEngineImageLayoutIndex(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
    vk::AccessFlagBits2 srcAccessMask, vk::AccessFlagBits2 dstAccessMask, vk::PipelineStageFlagBits2 srcStageMask,
    vk::PipelineStageFlagBits2 dstStageMask)
{
    // transition using pipeline barrier
    vk::ImageMemoryBarrier2 barrier
    {
        .srcStageMask = srcStageMask,          // Pipeline stage that must complete before barrier
        .srcAccessMask = srcAccessMask,         // Memory accesses that must complete before barrier
        .dstStageMask = dstStageMask,          // Pipeline stage that must wait on barrier
        .dstAccessMask = dstAccessMask,          // Memory accesses that must wait on barrier
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapChainImages[imageIndex],
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vk::DependencyInfo dependencyInfo
    {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    commandBuffers[currentFrame].pipelineBarrier2(dependencyInfo);
}

void AnubisEngine::transitionEngineImageLayoutImage(vk::raii::Image& image, vk::ImageLayout old_layout,
    vk::ImageLayout new_layout, vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags aspect_mask)
{
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = src_stage_mask,
        .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask,
        .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vk::DependencyInfo dependency_info = {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    commandBuffers[currentFrame].pipelineBarrier2(dependency_info);
}

void AnubisEngine::compileShader(std::string filename, std::string target, std::string profile, std::string vertEntry,
                                 std::string fragEntry, std::string outputName)
{
    // shader.slang
    // spirv
    // spirv_1_4
    // vertMain
    // fragMain
    // slang.spv
    Logger::printToConsole("***** Compiling Shader [" + filename + "] *****");
    Logger::printToConsole("Target: " + target);
    Logger::printToConsole("Profile: " + profile);
    Logger::printToConsole("Vert Entry: " + vertEntry);
    Logger::printToConsole("Frag Entry: " + fragEntry);
    Logger::printToConsole("Output Name: " + outputName);

    std::string shaderFilePath = "shaders\\" + filename ;
    std::string outputPath = "shaders\\" + outputName;
    std::string batPath = "shaders\\compile_shader.bat";
    std::string compileCommand = batPath + " \"" + shaderFilePath + "\" \"" + target + "\" \"" + profile + "\" \"" + vertEntry + "\" \"" + fragEntry + "\" \"" + outputPath + "\"";
    Logger::printToConsole(compileCommand);
    
    int result = system(compileCommand.c_str());
    if (result != 0)
        Logger::printToConsole("Failed to compile shader: " + std::to_string(result), level::err);
    else
        Logger::printToConsole("Successfully compiled shader!");
    
    Logger::printToConsole("*************************");
}
