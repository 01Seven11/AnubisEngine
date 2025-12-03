#pragma once
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;

    // describes the rate at which to load data from memory throughout the verts
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        // binding = index of the binding in an array of bindings
        // stride = number of bits from one entry to the next,
        // inputRate =
        //  eVertex: move to next after each vert.
        //  eInstance: move to next after each instance.
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        // binding = which binding the per-vert data comes from
        // location = references the 'location' directive of the input in the vertex shader
        //  in shader.slang 0 = float2 inPosition, 1 = float3 inColor
        // formats:
        //  common:
        /*
                float: VK_FORMAT_R32_SFLOAT
                float2: VK_FORMAT_R32G32_SFLOAT
                float3: VK_FORMAT_R32G32B32_SFLOAT
                float4: VK_FORMAT_R32G32B32A32_SFLOAT
                int2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
                uint4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
                double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
        */
        // offset = the number of bytes since the start of the per-vertex data to read from
        // match to the shader input for the verts
        return {
            vk::VertexInputAttributeDescription(0,0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1,0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2,0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
        };
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && uv == other.uv;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.uv) << 1);
        }
    };
}

class GeneratedShapes
{
public:
    // uint16_t for unique vert count < 65535
    // uint32_t for unique vert count < 4294967295
    static std::tuple<std::vector<Vertex>, std::vector<uint32_t>> getTriangle()
    {
        std::vector<Vertex> vertices = {
            {{0.0f, -0.5f, 0.0},{1.0f, 0.0f, 0.0f}, {1.0, 0.0}},
            {{0.5f, 0.5f, 0.0},{0.0f, 1.0f, 0.0f}, {0.0,0.0}},
            {{-0.5f, 0.5f, 0.0},{0.0f, 0.0f, 1.0f}, {0.0, 1.0}},
        };
        std::vector<uint32_t> indices = {0, 1, 2};
        return make_tuple(vertices, indices);
    }

    static std::tuple<std::vector<Vertex>, std::vector<uint32_t>> getRectangle()
    {
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0},{1.0f, 0.0f, 0.0f},{1.0, 0.0}},
            {{0.5f, -0.5f, 0.0},{0.0f, 1.0f, 0.0f}, {0.0, 0.0}},
            {{0.5f, 0.5f, 0.0},{0.0f, 0.0f, 1.0f}, {0.0, 1.0}},
            {{-0.5f, 0.5f, 0.0},{1.0, 1.0f, 1.0f},{1.0, 1.0}}
        };
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        return make_tuple(vertices, indices);
    }

    static std::tuple<std::vector<Vertex>, std::vector<uint32_t>> getDualRectangle()
    {
        std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f, 0.0},{1.0f, 0.0f, 0.0f},{1.0, 0.0}},
            {{0.5f, -0.5f, 0.0},{0.0f, 1.0f, 0.0f}, {0.0, 0.0}},
            {{0.5f, 0.5f, 0.0},{0.0f, 0.0f, 1.0f}, {0.0, 1.0}},
            {{-0.5f, 0.5f, 0.0},{1.0, 1.0f, 1.0f},{1.0, 1.0}},

            {{-0.5f, -0.5f, -0.5},{1.0f, 0.0f, 0.0f},{1.0, 0.0}},
            {{0.5f, -0.5f, -0.5},{0.0f, 1.0f, 0.0f}, {0.0, 0.0}},
            {{0.5f, 0.5f, -0.5},{0.0f, 0.0f, 1.0f}, {0.0, 1.0}},
            {{-0.5f, 0.5f, -0.5},{1.0, 1.0f, 1.0f},{1.0, 1.0}}
        };
        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        };
        return make_tuple(vertices, indices);
    }
};
