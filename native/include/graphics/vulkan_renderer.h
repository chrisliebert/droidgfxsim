// Copyright (C) 2017-2026 Chris Liebert

#ifndef _VULKAN_RENDERER_H_
#define _VULKAN_RENDERER_H_

#include "graphics/renderer_interface.h"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

// Forward declaration of GLFWwindow
struct GLFWwindow;

#if defined(DROIDGFXSIM_HAS_VULKAN) || (defined(__has_include) && __has_include(<vulkan/vulkan.h>))
#ifndef DROIDGFXSIM_HAS_VULKAN
#define DROIDGFXSIM_HAS_VULKAN 1
#endif
#include <vulkan/vulkan.h>
#else
// Stand-in definitions when Vulkan headers are not present
typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkSwapchainKHR_T* VkSwapchainKHR;
typedef struct VkImage_T* VkImage;
typedef struct VkImageView_T* VkImageView;
typedef struct VkRenderPass_T* VkRenderPass;
typedef struct VkPipelineLayout_T* VkPipelineLayout;
typedef struct VkPipeline_T* VkPipeline;
typedef struct VkFramebuffer_T* VkFramebuffer;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkCommandBuffer_T* VkCommandBuffer;
typedef struct VkSemaphore_T* VkSemaphore;
typedef struct VkFence_T* VkFence;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkBuffer_T* VkBuffer;
typedef uint32_t VkFormat;
typedef uint32_t VkMemoryPropertyFlags;
typedef uint32_t VkBufferUsageFlags;
typedef uint64_t VkDeviceSize;
typedef struct VkExtent2D {
    uint32_t width;
    uint32_t height;
} VkExtent2D;
#endif

namespace scenegraph {
    class Node;
    class GeometryNode;
}

class Camera;

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    virtual ~VulkanRenderer() override;

    bool init(GLFWwindow* window = nullptr);
    void cleanup();
    virtual void render(scenegraph::Node* node, Camera* camera) override;
    static bool isSupported();

    // Helper initialization methods
    bool createInstance();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapChain();
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();

    // Required member variables per specification
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

private:
    GLFWwindow* window;
    bool isHeadless;
    bool initialized;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
    VkExtent2D swapChainExtent;
    VkFormat swapChainImageFormat;

    // Headless / offscreen support resources
    VkImage offscreenImage;
    VkDeviceMemory offscreenImageMemory;
    VkImageView offscreenImageView;

    // Mesh buffer cache
    struct MeshBuffer {
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
        uint32_t indexCount;
        uint32_t vertexCount;
    };
    std::map<scenegraph::GeometryNode*, MeshBuffer> meshBuffers;

    uint32_t findMemoryType(uint32_t typeFilter, uint32_t properties);
    void createBuffer(uint64_t size, uint32_t usage, uint32_t properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void uploadBufferData(VkDeviceMemory memory, const void* data, uint64_t size);
    void ensureMeshBuffers(scenegraph::GeometryNode* geomNode);
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, scenegraph::Node* rootNode, Camera* camera);
    void recordNodeDraw(VkCommandBuffer cmd, scenegraph::Node* node, const void* parentMatrix, const void* viewProjMatrix);
};

#endif // _VULKAN_RENDERER_H_
