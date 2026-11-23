// Copyright (C) 2017-2026 Chris Liebert

#include "graphics/vulkan_renderer.h"
#include "common/log.h"
#include "graphics/scene_graph.h"
#include "graphics/camera.h"

#include <iostream>
#include <vector>
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(DESKTOP_APP) && !defined(__ANDROID__)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

namespace {

// Push constant block layout matching vertex shader
struct PushConstantBlock {
    glm::mat4 mvp;
    glm::mat4 model;
};

// Precompiled SPIR-V 1.3 vertex shader bytecode
// Source: GLSL 450 vertex shader with push constants (mvp, model) & Vertex attributes (pos, normal, uv)
const uint32_t kVertexShaderSpv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000039, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x000b000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00000019, 0x00000024,
    0x00000030, 0x00000035, 0x00000037, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004,
    0x6e69616d, 0x00000000, 0x00060005, 0x0000000b, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000,
    0x00060006, 0x0000000b, 0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006, 0x0000000b,
    0x00000001, 0x505f6c67, 0x746e696f, 0x657a6953, 0x00000000, 0x00070006, 0x0000000b, 0x00000002,
    0x435f6c67, 0x4470696c, 0x61747369, 0x0065636e, 0x00070006, 0x0000000b, 0x00000003, 0x435f6c67,
    0x446c6c75, 0x61747369, 0x0065636e, 0x00030005, 0x0000000d, 0x00000000, 0x00050005, 0x00000011,
    0x68737550, 0x736e6f43, 0x00007374, 0x00040006, 0x00000011, 0x00000000, 0x0070766d, 0x00050006,
    0x00000011, 0x00000001, 0x65646f6d, 0x0000006c, 0x00040005, 0x00000013, 0x68737570, 0x00000000,
    0x00050005, 0x00000019, 0x6f506e69, 0x69746973, 0x00006e6f, 0x00050005, 0x00000024, 0x67617266,
    0x6d726f4e, 0x00006c61, 0x00050005, 0x00000030, 0x6f4e6e69, 0x6c616d72, 0x00000000, 0x00060005,
    0x00000035, 0x67617266, 0x43786554, 0x64726f6f, 0x00000000, 0x00050005, 0x00000037, 0x65546e69,
    0x6f6f4378, 0x00006472, 0x00030047, 0x0000000b, 0x00000002, 0x00050048, 0x0000000b, 0x00000000,
    0x0000000b, 0x00000000, 0x00050048, 0x0000000b, 0x00000001, 0x0000000b, 0x00000001, 0x00050048,
    0x0000000b, 0x00000002, 0x0000000b, 0x00000003, 0x00050048, 0x0000000b, 0x00000003, 0x0000000b,
    0x00000004, 0x00030047, 0x00000011, 0x00000002, 0x00040048, 0x00000011, 0x00000000, 0x00000005,
    0x00050048, 0x00000011, 0x00000000, 0x00000007, 0x00000010, 0x00050048, 0x00000011, 0x00000000,
    0x00000023, 0x00000000, 0x00040048, 0x00000011, 0x00000001, 0x00000005, 0x00050048, 0x00000011,
    0x00000001, 0x00000007, 0x00000010, 0x00050048, 0x00000011, 0x00000001, 0x00000023, 0x00000040,
    0x00040047, 0x00000019, 0x0000001e, 0x00000000, 0x00040047, 0x00000024, 0x0000001e, 0x00000000,
    0x00040047, 0x00000030, 0x0000001e, 0x00000001, 0x00040047, 0x00000035, 0x0000001e, 0x00000001,
    0x00040047, 0x00000037, 0x0000001e, 0x00000002, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
    0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004,
    0x00040015, 0x00000008, 0x00000020, 0x00000000, 0x0004002b, 0x00000008, 0x00000009, 0x00000001,
    0x0004001c, 0x0000000a, 0x00000006, 0x00000009, 0x0006001e, 0x0000000b, 0x00000007, 0x00000006,
    0x0000000a, 0x0000000a, 0x00040020, 0x0000000c, 0x00000003, 0x0000000b, 0x0004003b, 0x0000000c,
    0x0000000d, 0x00000003, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e,
    0x0000000f, 0x00000000, 0x00040018, 0x00000010, 0x00000007, 0x00000004, 0x0004001e, 0x00000011,
    0x00000010, 0x00000010, 0x00040020, 0x00000012, 0x00000009, 0x00000011, 0x0004003b, 0x00000012,
    0x00000013, 0x00000009, 0x00040020, 0x00000014, 0x00000009, 0x00000010, 0x00040017, 0x00000017,
    0x00000006, 0x00000003, 0x00040020, 0x00000018, 0x00000001, 0x00000017, 0x0004003b, 0x00000018,
    0x00000019, 0x00000001, 0x0004002b, 0x00000006, 0x0000001b, 0x3f800000, 0x00040020, 0x00000021,
    0x00000003, 0x00000007, 0x00040020, 0x00000023, 0x00000003, 0x00000017, 0x0004003b, 0x00000023,
    0x00000024, 0x00000003, 0x0004002b, 0x0000000e, 0x00000025, 0x00000001, 0x00040018, 0x00000028,
    0x00000017, 0x00000003, 0x0004003b, 0x00000018, 0x00000030, 0x00000001, 0x00040017, 0x00000033,
    0x00000006, 0x00000002, 0x00040020, 0x00000034, 0x00000003, 0x00000033, 0x0004003b, 0x00000034,
    0x00000035, 0x00000003, 0x00040020, 0x00000036, 0x00000001, 0x00000033, 0x0004003b, 0x00000036,
    0x00000037, 0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
    0x00000005, 0x00050041, 0x00000014, 0x00000015, 0x00000013, 0x0000000f, 0x0004003d, 0x00000010,
    0x00000016, 0x00000015, 0x0004003d, 0x00000017, 0x0000001a, 0x00000019, 0x00050051, 0x00000006,
    0x0000001c, 0x0000001a, 0x00000000, 0x00050051, 0x00000006, 0x0000001d, 0x0000001a, 0x00000001,
    0x00050051, 0x00000006, 0x0000001e, 0x0000001a, 0x00000002, 0x00070050, 0x00000007, 0x0000001f,
    0x0000001c, 0x0000001d, 0x0000001e, 0x0000001b, 0x00050091, 0x00000007, 0x00000020, 0x00000016,
    0x0000001f, 0x00050041, 0x00000021, 0x00000022, 0x0000000d, 0x0000000f, 0x0003003e, 0x00000022,
    0x00000020, 0x00050041, 0x00000014, 0x00000026, 0x00000013, 0x00000025, 0x0004003d, 0x00000010,
    0x00000027, 0x00000026, 0x00050051, 0x00000007, 0x00000029, 0x00000027, 0x00000000, 0x0008004f,
    0x00000017, 0x0000002a, 0x00000029, 0x00000029, 0x00000000, 0x00000001, 0x00000002, 0x00050051,
    0x00000007, 0x0000002b, 0x00000027, 0x00000001, 0x0008004f, 0x00000017, 0x0000002c, 0x0000002b,
    0x0000002b, 0x00000000, 0x00000001, 0x00000002, 0x00050051, 0x00000007, 0x0000002d, 0x00000027,
    0x00000002, 0x0008004f, 0x00000017, 0x0000002e, 0x0000002d, 0x0000002d, 0x00000000, 0x00000001,
    0x00000002, 0x00060050, 0x00000028, 0x0000002f, 0x0000002a, 0x0000002c, 0x0000002e, 0x0004003d,
    0x00000017, 0x00000031, 0x00000030, 0x00050091, 0x00000017, 0x00000032, 0x0000002f, 0x00000031,
    0x0003003e, 0x00000024, 0x00000032, 0x0004003d, 0x00000033, 0x00000038, 0x00000037, 0x0003003e,
    0x00000035, 0x00000038, 0x000100fd, 0x00010038
};

const uint32_t kFragmentShaderSpv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000025, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000011, 0x0000001a, 0x00000024,
    0x00030010, 0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004,
    0x6e69616d, 0x00000000, 0x00050005, 0x00000009, 0x6867696c, 0x72694474, 0x00000000, 0x00040005,
    0x0000000f, 0x66666964, 0x00000000, 0x00050005, 0x00000011, 0x67617266, 0x6d726f4e, 0x00006c61,
    0x00050005, 0x0000001a, 0x4374756f, 0x726f6c6f, 0x00000000, 0x00060005, 0x00000024, 0x67617266,
    0x43786554, 0x64726f6f, 0x00000000, 0x00040047, 0x00000011, 0x0000001e, 0x00000000, 0x00040047,
    0x0000001a, 0x0000001e, 0x00000000, 0x00040047, 0x00000024, 0x0000001e, 0x00000001, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017,
    0x00000007, 0x00000006, 0x00000003, 0x00040020, 0x00000008, 0x00000007, 0x00000007, 0x0004002b,
    0x00000006, 0x0000000a, 0x3edd267b, 0x0004002b, 0x00000006, 0x0000000b, 0x3f5d267b, 0x0004002b,
    0x00000006, 0x0000000c, 0x3e84b0b0, 0x0006002c, 0x00000007, 0x0000000d, 0x0000000a, 0x0000000b,
    0x0000000c, 0x00040020, 0x0000000e, 0x00000007, 0x00000006, 0x00040020, 0x00000010, 0x00000001,
    0x00000007, 0x0004003b, 0x00000010, 0x00000011, 0x00000001, 0x0004002b, 0x00000006, 0x00000016,
    0x3e4ccccd, 0x00040017, 0x00000018, 0x00000006, 0x00000004, 0x00040020, 0x00000019, 0x00000003,
    0x00000018, 0x0004003b, 0x00000019, 0x0000001a, 0x00000003, 0x0004002b, 0x00000006, 0x0000001d,
    0x3f800000, 0x00040017, 0x00000022, 0x00000006, 0x00000002, 0x00040020, 0x00000023, 0x00000001,
    0x00000022, 0x0004003b, 0x00000023, 0x00000024, 0x00000001, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003b, 0x00000008, 0x00000009, 0x00000007,
    0x0004003b, 0x0000000e, 0x0000000f, 0x00000007, 0x0003003e, 0x00000009, 0x0000000d, 0x0004003d,
    0x00000007, 0x00000012, 0x00000011, 0x0006000c, 0x00000007, 0x00000013, 0x00000001, 0x00000045,
    0x00000012, 0x0004003d, 0x00000007, 0x00000014, 0x00000009, 0x00050094, 0x00000006, 0x00000015,
    0x00000013, 0x00000014, 0x0007000c, 0x00000006, 0x00000017, 0x00000001, 0x00000028, 0x00000015,
    0x00000016, 0x0003003e, 0x0000000f, 0x00000017, 0x0004003d, 0x00000006, 0x0000001b, 0x0000000f,
    0x00060050, 0x00000007, 0x0000001c, 0x0000001b, 0x0000001b, 0x0000001b, 0x00050051, 0x00000006,
    0x0000001e, 0x0000001c, 0x00000000, 0x00050051, 0x00000006, 0x0000001f, 0x0000001c, 0x00000001,
    0x00050051, 0x00000006, 0x00000020, 0x0000001c, 0x00000002, 0x00070050, 0x00000018, 0x00000021,
    0x0000001e, 0x0000001f, 0x00000020, 0x0000001d, 0x0003003e, 0x0000001a, 0x00000021, 0x000100fd,
    0x00010038
};

} // namespace

VulkanRenderer::VulkanRenderer()
    : instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , device(VK_NULL_HANDLE)
    , graphicsQueue(VK_NULL_HANDLE)
    , presentQueue(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , swapChain(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
    , pipelineLayout(VK_NULL_HANDLE)
    , graphicsPipeline(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE)
    , imageAvailableSemaphore(VK_NULL_HANDLE)
    , renderFinishedSemaphore(VK_NULL_HANDLE)
    , inFlightFence(VK_NULL_HANDLE)
    , window(nullptr)
    , isHeadless(true)
    , initialized(false)
    , graphicsQueueFamilyIndex(0)
    , presentQueueFamilyIndex(0)
    , swapChainExtent{1200, 800}
#if defined(DROIDGFXSIM_HAS_VULKAN)
    , swapChainImageFormat(VK_FORMAT_B8G8R8A8_UNORM)
#else
    , swapChainImageFormat(0)
#endif
    , offscreenImage(VK_NULL_HANDLE)
    , offscreenImageMemory(VK_NULL_HANDLE)
    , offscreenImageView(VK_NULL_HANDLE)
{
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::isSupported() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    uint32_t extensionCount = 0;
    VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (res != VK_SUCCESS) {
        LOGI("Vulkan probe: vkEnumerateInstanceExtensionProperties returned %d", res);
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VulkanSupportProbe";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "droidgfxsim";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance testInstance = VK_NULL_HANDLE;
    res = vkCreateInstance(&createInfo, nullptr, &testInstance);
    if (res != VK_SUCCESS) {
        LOGI("Vulkan probe: instance creation returned %d", res);
        return false;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(testInstance, &deviceCount, nullptr);
    vkDestroyInstance(testInstance, nullptr);

    if (deviceCount == 0) {
        LOGI("Vulkan probe: 0 physical devices found.");
        return false;
    }

    LOGI("Vulkan is supported (%u physical devices found)", deviceCount);
    return true;
#else
    LOGI("Vulkan is disabled or headers are missing at compile time.");
    return false;
#endif
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, uint32_t properties) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOGE("Failed to find suitable memory type for Vulkan buffer/image (filter: 0x%x, props: 0x%x)", typeFilter, properties);
#endif
    return 0;
}

void VulkanRenderer::createBuffer(uint64_t size, uint32_t usage, uint32_t properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult res = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan buffer of size %llu (VkResult %d)", static_cast<unsigned long long>(size), res);
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    res = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    if (res != VK_SUCCESS) {
        LOGE("Failed to allocate buffer memory (VkResult %d)", res);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return;
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
#endif
}

void VulkanRenderer::uploadBufferData(VkDeviceMemory memory, const void* data, uint64_t size) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!memory || !data || size == 0) return;
    void* mapped = nullptr;
    VkResult res = vkMapMemory(device, memory, 0, size, 0, &mapped);
    if (res == VK_SUCCESS && mapped) {
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(device, memory);
    } else {
        LOGE("Failed to map memory for buffer upload (VkResult %d)", res);
    }
#endif
}

void VulkanRenderer::ensureMeshBuffers(scenegraph::GeometryNode* geomNode) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!geomNode || geomNode->vertex_data.empty()) return;
    auto it = meshBuffers.find(geomNode);
    if (it != meshBuffers.end()) {
        return;
    }

    MeshBuffer mb{};
    mb.vertexCount = static_cast<uint32_t>(geomNode->vertex_data.size());
    VkDeviceSize vertexBufferSize = sizeof(scenegraph::Vertex) * geomNode->vertex_data.size();

    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 mb.vertexBuffer,
                 mb.vertexBufferMemory);

    uploadBufferData(mb.vertexBufferMemory, geomNode->vertex_data.data(), vertexBufferSize);

    if (!geomNode->index_data.empty()) {
        mb.indexCount = static_cast<uint32_t>(geomNode->index_data.size());
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * geomNode->index_data.size();

        createBuffer(indexBufferSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     mb.indexBuffer,
                     mb.indexBufferMemory);

        uploadBufferData(mb.indexBufferMemory, geomNode->index_data.data(), indexBufferSize);
    }

    meshBuffers[geomNode] = mb;
#endif
}

bool VulkanRenderer::createInstance() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "droidgfxsim";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "droidgfxsim_engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> requiredExtensions;
    if (!isHeadless && window != nullptr) {
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        if (glfwExts && glfwExtCount > 0) {
            for (uint32_t i = 0; i < glfwExtCount; i++) {
                requiredExtensions.push_back(glfwExts[i]);
            }
        } else {
            LOGI("GLFW required instance extensions not available; falling back to headless Vulkan");
            isHeadless = true;
        }
#endif
    }

    uint32_t availableExtCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, availableExtensions.data());

    std::vector<const char*> validExtensions;
    for (const char* req : requiredExtensions) {
        bool found = false;
        for (const auto& ext : availableExtensions) {
            if (std::strcmp(req, ext.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            validExtensions.push_back(req);
        } else {
            LOGI("Instance extension %s not supported by loader", req);
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(validExtensions.size());
    createInfo.ppEnabledExtensionNames = validExtensions.empty() ? nullptr : validExtensions.data();
    createInfo.enabledLayerCount = 0;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        // Fallback to Vulkan 1.0 apiVersion if 1.3 unsupported by older loader
        appInfo.apiVersion = VK_API_VERSION_1_0;
        res = vkCreateInstance(&createInfo, nullptr, &instance);
    }

    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan instance (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan instance created successfully (API version 1.3 target)");

    if (!isHeadless && window != nullptr) {
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (res != VK_SUCCESS) {
            LOGI("Failed to create GLFW window surface (%d); falling back to headless mode", res);
            surface = VK_NULL_HANDLE;
            isHeadless = true;
        } else {
            LOGI("GLFW Vulkan window surface created successfully");
        }
#else
        isHeadless = true;
#endif
    }

    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::pickPhysicalDevice() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("Failed to find GPUs with Vulkan support");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    physicalDevice = VK_NULL_HANDLE;

    for (const auto& deviceCandidate : devices) {
        VkPhysicalDeviceProperties props;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(deviceCandidate, &props);
        vkGetPhysicalDeviceFeatures(deviceCandidate, &features);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(deviceCandidate, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(deviceCandidate, &queueFamilyCount, queueFamilies.data());

        int gIndex = -1;
        int pIndex = -1;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                gIndex = static_cast<int>(i);
            }
            if (!isHeadless && surface != VK_NULL_HANDLE) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(deviceCandidate, i, surface, &presentSupport);
                if (presentSupport) {
                    pIndex = static_cast<int>(i);
                }
            }
        }

        if (gIndex >= 0 && (isHeadless || pIndex >= 0)) {
            physicalDevice = deviceCandidate;
            graphicsQueueFamilyIndex = static_cast<uint32_t>(gIndex);
            presentQueueFamilyIndex = (pIndex >= 0) ? static_cast<uint32_t>(pIndex) : graphicsQueueFamilyIndex;
            LOGI("Selected Vulkan physical device: %s (Type: %d)", props.deviceName, props.deviceType);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        if (!isHeadless) {
            LOGI("No device supported surface presentation; switching to headless offscreen rendering");
            isHeadless = true;
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
                surface = VK_NULL_HANDLE;
            }
            return pickPhysicalDevice();
        }
        LOGE("Failed to find a suitable Vulkan physical device");
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createLogicalDevice() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions;
    if (!isHeadless && surface != VK_NULL_HANDLE) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();

    VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan logical device (VkResult %d)", res);
        return false;
    }

    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
    if (!isHeadless && surface != VK_NULL_HANDLE) {
        vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
    } else {
        presentQueue = graphicsQueue;
    }

    LOGI("Vulkan logical device and queues initialized successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createSwapChain() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (isHeadless || surface == VK_NULL_HANDLE) {
        // Headless offscreen rendering mode setup
        swapChainExtent = { 1200, 800 };
        swapChainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapChainImageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        VkResult res = vkCreateImage(device, &imageInfo, nullptr, &offscreenImage);
        if (res != VK_SUCCESS) {
            LOGE("Failed to create offscreen image (VkResult %d)", res);
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, offscreenImage, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        res = vkAllocateMemory(device, &allocInfo, nullptr, &offscreenImageMemory);
        if (res != VK_SUCCESS) {
            LOGE("Failed to allocate offscreen image memory (VkResult %d)", res);
            return false;
        }

        vkBindImageMemory(device, offscreenImage, offscreenImageMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = offscreenImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        res = vkCreateImageView(device, &viewInfo, nullptr, &offscreenImageView);
        if (res != VK_SUCCESS) {
            LOGE("Failed to create offscreen image view (VkResult %d)", res);
            return false;
        }

        swapChainImages.clear();
        swapChainImages.push_back(offscreenImage);
        swapChainImageViews.clear();
        swapChainImageViews.push_back(offscreenImageView);
        LOGI("Headless Vulkan offscreen framebuffer target created (%ux%u)", swapChainExtent.width, swapChainExtent.height);
        return true;
    }

    // Windowed presentation mode setup
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    }

    VkSurfaceFormatKHR surfaceFormat = formats.empty() ?
        VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }
    swapChainImageFormat = surfaceFormat.format;

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() && capabilities.currentExtent.width > 0) {
        swapChainExtent = capabilities.currentExtent;
    } else {
        int width = 1200, height = 800;
#if defined(DESKTOP_APP) && !defined(__ANDROID__)
        if (window) {
            glfwGetFramebufferSize(window, &width, &height);
        }
#endif
        swapChainExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        swapChainExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
    if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan swap chain (%d); falling back to headless mode", res);
        isHeadless = true;
        return createSwapChain();
    }

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, nullptr);
    swapChainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &actualImageCount, swapChainImages.data());

    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        res = vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]);
        if (res != VK_SUCCESS) {
            LOGE("Failed to create swap chain image view %zu (VkResult %d)", i, res);
            return false;
        }
    }

    LOGI("Vulkan swapchain initialized (%zu images, extent: %ux%u)", swapChainImages.size(), swapChainExtent.width, swapChainExtent.height);
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createRenderPass() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = (!isHeadless && surface != VK_NULL_HANDLE) ?
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult res = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan render pass (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan render pass created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createGraphicsPipeline() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkShaderModuleCreateInfo vertModuleInfo{};
    vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertModuleInfo.codeSize = sizeof(kVertexShaderSpv);
    vertModuleInfo.pCode = kVertexShaderSpv;

    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertShaderModule);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create vertex shader module (VkResult %d)", res);
        return false;
    }

    VkShaderModuleCreateInfo fragModuleInfo{};
    fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragModuleInfo.codeSize = sizeof(kFragmentShaderSpv);
    fragModuleInfo.pCode = kFragmentShaderSpv;

    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    res = vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragShaderModule);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create fragment shader module (VkResult %d)", res);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(scenegraph::Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[3]{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, position));

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, normal));

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = static_cast<uint32_t>(offsetof(scenegraph::Vertex, texcoord));

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantBlock);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    res = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create pipeline layout (VkResult %d)", res);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    if (res != VK_SUCCESS) {
        LOGE("Failed to create graphics pipeline (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan graphics pipeline created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createFramebuffers() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        VkResult res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
        if (res != VK_SUCCESS) {
            LOGE("Failed to create framebuffer %zu (VkResult %d)", i, res);
            return false;
        }
    }

    LOGI("Created %zu Vulkan framebuffers", swapChainFramebuffers.size());
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createCommandPool() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

    VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create Vulkan command pool (VkResult %d)", res);
        return false;
    }

    LOGI("Vulkan command pool created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createCommandBuffers() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult res = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    if (res != VK_SUCCESS) {
        LOGE("Failed to allocate command buffers (VkResult %d)", res);
        return false;
    }

    LOGI("Allocated %zu Vulkan command buffers", commandBuffers.size());
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::createSyncObjects() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult r1 = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
    VkResult r2 = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore);
    VkResult r3 = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence);

    if (r1 != VK_SUCCESS || r2 != VK_SUCCESS || r3 != VK_SUCCESS) {
        LOGE("Failed to create Vulkan sync objects (%d, %d, %d)", r1, r2, r3);
        return false;
    }

    LOGI("Vulkan synchronization objects created successfully");
    return true;
#else
    return false;
#endif
}

bool VulkanRenderer::init(GLFWwindow* win) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    cleanup();
    this->window = win;
    this->isHeadless = (win == nullptr);

    LOGI("Initializing Vulkan 1.3 IRenderer backend (%s mode)...", isHeadless ? "Headless Offscreen" : "Windowed");

    if (!createInstance()) {
        LOGE("Vulkan initialization failed at createInstance");
        cleanup();
        return false;
    }
    if (!pickPhysicalDevice()) {
        LOGE("Vulkan initialization failed at pickPhysicalDevice");
        cleanup();
        return false;
    }
    if (!createLogicalDevice()) {
        LOGE("Vulkan initialization failed at createLogicalDevice");
        cleanup();
        return false;
    }
    if (!createSwapChain()) {
        LOGE("Vulkan initialization failed at createSwapChain");
        cleanup();
        return false;
    }
    if (!createRenderPass()) {
        LOGE("Vulkan initialization failed at createRenderPass");
        cleanup();
        return false;
    }
    if (!createGraphicsPipeline()) {
        LOGE("Vulkan initialization failed at createGraphicsPipeline");
        cleanup();
        return false;
    }
    if (!createFramebuffers()) {
        LOGE("Vulkan initialization failed at createFramebuffers");
        cleanup();
        return false;
    }
    if (!createCommandPool()) {
        LOGE("Vulkan initialization failed at createCommandPool");
        cleanup();
        return false;
    }
    if (!createCommandBuffers()) {
        LOGE("Vulkan initialization failed at createCommandBuffers");
        cleanup();
        return false;
    }
    if (!createSyncObjects()) {
        LOGE("Vulkan initialization failed at createSyncObjects");
        cleanup();
        return false;
    }

    initialized = true;
    LOGI("Vulkan 1.3 IRenderer successfully initialized and ready for draw passes");
    return true;
#else
    LOGE("Cannot initialize VulkanRenderer: Vulkan is disabled or headers are missing");
    return false;
#endif
}

void VulkanRenderer::cleanup() {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        for (auto& pair : meshBuffers) {
            if (pair.second.vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, pair.second.vertexBuffer, nullptr);
            }
            if (pair.second.vertexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, pair.second.vertexBufferMemory, nullptr);
            }
            if (pair.second.indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, pair.second.indexBuffer, nullptr);
            }
            if (pair.second.indexBufferMemory != VK_NULL_HANDLE) {
                vkFreeMemory(device, pair.second.indexBufferMemory, nullptr);
            }
        }
        meshBuffers.clear();

        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFence, nullptr);
            inFlightFence = VK_NULL_HANDLE;
        }

        if (commandPool != VK_NULL_HANDLE) {
            if (!commandBuffers.empty()) {
                vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
                commandBuffers.clear();
            }
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }

        for (auto fb : swapChainFramebuffers) {
            if (fb != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device, fb, nullptr);
            }
        }
        swapChainFramebuffers.clear();

        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
            graphicsPipeline = VK_NULL_HANDLE;
        }
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            pipelineLayout = VK_NULL_HANDLE;
        }
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
            renderPass = VK_NULL_HANDLE;
        }

        for (auto iv : swapChainImageViews) {
            if (iv != VK_NULL_HANDLE && iv != offscreenImageView) {
                vkDestroyImageView(device, iv, nullptr);
            }
        }
        swapChainImageViews.clear();

        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            swapChain = VK_NULL_HANDLE;
        }

        if (offscreenImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, offscreenImageView, nullptr);
            offscreenImageView = VK_NULL_HANDLE;
        }
        if (offscreenImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, offscreenImage, nullptr);
            offscreenImage = VK_NULL_HANDLE;
        }
        if (offscreenImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, offscreenImageMemory, nullptr);
            offscreenImageMemory = VK_NULL_HANDLE;
        }

        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    swapChainImages.clear();
    physicalDevice = VK_NULL_HANDLE;
    graphicsQueue = VK_NULL_HANDLE;
    presentQueue = VK_NULL_HANDLE;
    window = nullptr;
    initialized = false;
#endif
}

void VulkanRenderer::recordNodeDraw(VkCommandBuffer cmd, scenegraph::Node* node, const void* parentMatrix, const void* viewProjMatrix) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!node) return;

    const glm::mat4& parentMat = *static_cast<const glm::mat4*>(parentMatrix);
    const glm::mat4& vpMat = *static_cast<const glm::mat4*>(viewProjMatrix);
    glm::mat4 currentModel = parentMat;

    if (node->type == scenegraph::NodeType::Transform) {
        auto* tn = static_cast<scenegraph::TransformNode*>(node);
        currentModel = parentMat * tn->matrix;
    } else if (node->type == scenegraph::NodeType::Geometry) {
        auto* gn = static_cast<scenegraph::GeometryNode*>(node);
        if (gn && !gn->vertex_data.empty()) {
            ensureMeshBuffers(gn);
            auto it = meshBuffers.find(gn);
            if (it != meshBuffers.end() && it->second.vertexBuffer != VK_NULL_HANDLE) {
                PushConstantBlock pc{};
                pc.model = currentModel;
                pc.mvp = vpMat * currentModel;

                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantBlock), &pc);

                VkBuffer vertexBuffers[] = { it->second.vertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

                if (it->second.indexBuffer != VK_NULL_HANDLE && it->second.indexCount > 0) {
                    vkCmdBindIndexBuffer(cmd, it->second.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmd, it->second.indexCount, 1, 0, 0, 0);
                } else {
                    vkCmdDraw(cmd, it->second.vertexCount, 1, 0, 0);
                }
            }
        }
    }

    for (scenegraph::Node* child : node->children) {
        recordNodeDraw(cmd, child, &currentModel, viewProjMatrix);
    }
#endif
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, scenegraph::Node* rootNode, Camera* camera) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        LOGE("Failed to begin recording Vulkan command buffer");
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {{{0.12f, 0.15f, 0.22f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    glm::mat4 viewProj = glm::mat4(1.0f);
    if (camera != nullptr) {
        glm::mat4 proj = camera->projection_matrix;
        proj[1][1] *= -1.0f; // Invert Vulkan clip-space Y coordinate relative to OpenGL
        viewProj = proj * camera->modelview_matrix;
    }

    glm::mat4 identityModel = glm::mat4(1.0f);
    if (rootNode != nullptr) {
        recordNodeDraw(cmd, rootNode, &identityModel, &viewProj);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOGE("Failed to finalize recording Vulkan command buffer");
    }
#endif
}

void VulkanRenderer::render(scenegraph::Node* node, Camera* camera) {
#if defined(DROIDGFXSIM_HAS_VULKAN)
    if (!initialized || device == VK_NULL_HANDLE) {
        return;
    }

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    if (!isHeadless && swapChain != VK_NULL_HANDLE) {
        uint32_t imageIndex = 0;
        VkResult res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            // Swap chain resize or recreation needed
            return;
        } else if (res != VK_SUCCESS) {
            LOGE("Failed to acquire next Vulkan swap chain image (VkResult %d)", res);
            return;
        }

        vkResetFences(device, 1, &inFlightFence);
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex, node, camera);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (res != VK_SUCCESS) {
            LOGE("Failed to submit draw command buffer to graphics queue (VkResult %d)", res);
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);
    } else {
        // Headless offscreen rendering mode pass
        vkResetFences(device, 1, &inFlightFence);
        uint32_t imageIndex = 0;
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex, node, camera);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

        VkResult res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (res != VK_SUCCESS) {
            LOGE("Failed to submit headless command buffer (VkResult %d)", res);
            return;
        }

        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    }
#endif
}
