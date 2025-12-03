#pragma once
#include <fstream>
#include <queue>

#include "Logger.h"

namespace helpers
{
    static std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            Logger::printToConsole("Failed to open file: " + filename, level::err);
            throw std::runtime_error("failed to open file: " + filename);
        }

        std::vector<char> buffer(file.tellg());
        Logger::printToConsole("File Size:" + std::to_string(file.tellg()));
        
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        file.close();

        return buffer;
    }

    static uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, const vk::raii::PhysicalDevice& physicalDevice)
    {
        // get the memory properties of the physical device
        vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();

        // iterate through the memory types
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            // if the type matches the filter and the properties match
            if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        Logger::printToConsole("failed to find suitable memory type!", level::err);
        throw std::runtime_error("failed to find suitable memory type!");
    }

    // TODO: need to update to handle a large number of objects at the same time
    //  create a custom allocator that splits up a single allocation among many different objects by using the offset parameters
    //  see: VulkanMemoryAllocator library
    static void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory, const
                             vk::raii::Device& logicalDevice, const vk::raii::PhysicalDevice& physicalDevice)
    {
        // create the buffer
        vk::BufferCreateInfo bufferInfo
        {
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive
        };
        buffer = vk::raii::Buffer(logicalDevice, bufferInfo);

        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo
    {
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, physicalDevice)
    };

        bufferMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);

        // associate the memory to the buffer
        // if offset is non-zero, then it is required to be divisible by memRequirements.alignment
        buffer.bindMemory(*bufferMemory, 0);
    }

    static void createImage(uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::
                            MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory, const vk::raii::Device& logicalDevice, const
                            vk::raii::PhysicalDevice& physicalDevice)
    {
        vk::Extent3D extent{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
        vk::ImageCreateInfo imageCreateInfo
        {
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = extent,
            .mipLevels = mipLevels,
            .arrayLayers = 1,
            .samples = numSamples,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0,
        };

        image = vk::raii::Image(logicalDevice, imageCreateInfo);

        vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo
        {
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = helpers::findMemoryType(memRequirements.memoryTypeBits, properties, physicalDevice)
        };
        imageMemory = vk::raii::DeviceMemory(logicalDevice, allocInfo);
        image.bindMemory(imageMemory, 0);
    }

    static vk::raii::CommandBuffer beginSingleTimeCommands(const vk::raii::CommandPool& commandPool, const vk::raii::Device& logicalDevice)
    {
        vk::CommandBufferAllocateInfo allocInfo
        {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        vk::raii::CommandBuffer commandBuffer = std::move(logicalDevice.allocateCommandBuffers(allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo
        {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        commandBuffer.begin(beginInfo);

        return commandBuffer;
    }

    static void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer, const vk::raii::Queue& queue)
    {
        commandBuffer.end();

        vk::SubmitInfo submitInfo
        {
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer
        };
        // run the command
        queue.submit(submitInfo, nullptr);
        // note: can also leverage waitForFences
        //  a fence could allow for the ability to schedule multiple transfers.
        //  which in turn might give the driver more opportunity to optimize.
        queue.waitIdle();
    }

    static void copyBuffer(const vk::raii::Buffer& srcBuffer, const vk::raii::Buffer& dstBuffer, vk::DeviceSize size, const vk::raii::CommandPool& commandPool, const vk::raii::Device& logicalDevice, const vk::raii::Queue& queue)
    {
        vk::raii::CommandBuffer commandCopyBuffer = helpers::beginSingleTimeCommands(commandPool, logicalDevice);

        // transfer src to dst
        commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, {vk::BufferCopy{0, 0, size}});

        helpers::endSingleTimeCommands(commandCopyBuffer, queue);
    }

    static bool hasStencilComponent(vk::Format format)
    {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    static void transitionImageLayoutTexture(const vk::raii::Image& image, vk::Format format, uint32_t mipLevels, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
        const vk::raii::CommandPool& commandPool, const vk::raii::Device& logicalDevice, const vk::raii::Queue& queue)
    {
        auto commandBuffer = beginSingleTimeCommands(commandPool, logicalDevice);

        vk::ImageMemoryBarrier barrier
        {
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1}
        };

        vk::PipelineStageFlags srcStage;
        vk::PipelineStageFlags dstStage;

        // There are two transitions we need to handle:
        // Undefined → transfer destination: transfer writes that don’t need to wait on anything
        // Transfer destination → shader reading: shader reads should wait on transfer writes,
        // specifically the shader reads in the fragment shader, because that’s where we’re going to use the texture

        //depth image
        if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
        {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

            if (helpers::hasStencilComponent(format))
            {
                barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        }
        else
        {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eTransfer;
        } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask =  vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask =  vk::AccessFlagBits::eShaderRead;

            srcStage = vk::PipelineStageFlagBits::eTransfer;
            dstStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;

            srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
            dstStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        } else {
            Logger::printToConsole("unsupported layout transition!", level::err);
            throw std::invalid_argument("unsupported layout transition!");
        }
        
        // specify which types of operations that involve the resource must happen before the barrier,
        // and which operations that involve the resource must wait on the barrier
        commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, nullptr, {barrier});
        endSingleTimeCommands(commandBuffer, queue);
    }

    static void copyBufferToImage(const vk::raii::Buffer& buffer, const vk::raii::Image& image, int width, int height, const vk::raii::CommandPool& commandPool, const vk::raii::Device& logicalDevice, const vk::raii::Queue& queue)
    {
        // create a command buffer
        auto commandBuffer = beginSingleTimeCommands(commandPool, logicalDevice);

        vk::BufferImageCopy region
        {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
        };

        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
        
        endSingleTimeCommands(commandBuffer, queue);
    }

    static vk::raii::ImageView createImageView(const vk::raii::Image& image, vk::Format format, uint32_t mipLevels, vk::ImageAspectFlags aspectFlags, const vk::raii::Device& logicalDevice)
    {
        // explicitly setting ARGB components
        // imageViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
        // imageViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
        // imageViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
        // imageViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;

        // single line
        // vk::ImageViewCreateInfo imageViewCreateInfo( {}, {}, vk::ImageViewType::e2D, swapChainImageFormat, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } );
        vk::ImageViewCreateInfo viewInfo
        {
            // specifies rendering to 2D screen
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components = {},
            // color without mipmaps and no multiple layers (which stereoscopic might need)
            .subresourceRange = vk::ImageSubresourceRange{aspectFlags, 0, mipLevels, 0, 1}
        };
        return vk::raii::ImageView(logicalDevice, viewInfo);
    }

    static vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features, const vk::raii::PhysicalDevice& physicalDevice)
    {
        for (const auto format : candidates)
        {
            vk::FormatProperties props = physicalDevice.getFormatProperties(format);
            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        Logger::printToConsole("failed to find supported format!", level::err);
        throw std::runtime_error("failed to find supported format!");
    }

    static vk::Format findDepthFormat(const vk::raii::PhysicalDevice& physicalDevice)
    {
        return findSupportedFormat(
            {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment, physicalDevice
        );
    }

    static void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, uint32_t mipLevels,
        int32_t texWidth, int32_t texHeight,
        const vk::raii::CommandPool& commandPool, const vk::raii::Device& logicalDevice, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Queue& queue)
    {
        //does the texture support blit
        vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
        if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
        {
            Logger::printToConsole("texture does not support blit!", level::err);
            throw std::runtime_error("texture does not support blit!");
        }
        
        vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands(commandPool, logicalDevice);

        vk::ImageMemoryBarrier barrier
        {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 1, 1, 0, 1}
        };

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, nullptr, {barrier});

            // define the regions for the blit operation
            vk::ArrayWrapper1D<vk::Offset3D, 2> offsets, dstOffsets;
            offsets[0] = vk::Offset3D{0, 0, 0};
            offsets[1] = vk::Offset3D{mipWidth, mipHeight, 1};
            dstOffsets[0] = vk::Offset3D{0, 0, 0};
            dstOffsets[1] = vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};

            vk::ImageBlit blit = {
                .srcSubresource = {},
                .srcOffsets = offsets,
                .dstSubresource = {},
                .dstOffsets = dstOffsets
            };
            blit.srcSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i - 1, 0, 1};
            blit.dstSubresource = vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, i, 0, 1};

            commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

            barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, nullptr, {barrier});

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        // transition the last mip map
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, nullptr, {barrier});
        
        endSingleTimeCommands(commandBuffer, queue);
    }

    static vk::SampleCountFlagBits getMaxUsableSampleCount(const vk::raii::PhysicalDevice& physicalDevice)
    {
        vk::PhysicalDeviceProperties props = physicalDevice.getProperties();
        vk::SampleCountFlags counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

        if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
        if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
        if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
        if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
        if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
        if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

        return vk::SampleCountFlagBits::e1;
    }
}
