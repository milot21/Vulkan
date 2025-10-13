#pragma once

#include "lve_window.hpp"

// std lib headers
#include <string>
#include <vector>

namespace lve {
    // Holds swap chain support info for a physical device
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities; //Surface capabilities (min/max image count, extent, etc.)
        std::vector<VkSurfaceFormatKHR> formats; //avail surface formats (color space, format)
        std::vector<VkPresentModeKHR> presentModes; // avail presentation modes (immediate or fifo)
    };

    //Stores indices of queue families that support required operations
    struct QueueFamilyIndices {
        uint32_t graphicsFamily;              // Index of graphics queue family
        uint32_t presentFamily;               // index of present queue family
        bool graphicsFamilyHasValue = false;  // if graphics family was found
        bool presentFamilyHasValue = false;   // if present family was found

        // Returns true if both required queue families were found
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    /**
     * LveDevice - Manages Vulkan device setup and resource creation
     * Handles Vulkan instance, physical/logical device selection, queues, and command pools
     */
    class LveDevice {
    public:
        const bool enableValidationLayers = true; //Enable validation layers
        // constructor: sets up complete Vulkan device with instance, surface, and queues
        LveDevice(LveWindow &window);

        // destructor: Cleans up all Vulkan resources in proper order
        ~LveDevice();

        // Prevent copying/moving to avoid double cleanup of V resources
        LveDevice(const LveDevice &) = delete;
        LveDevice &operator=(const LveDevice &) = delete;
        LveDevice(LveDevice &&) = delete;
        LveDevice &operator=(LveDevice &&) = delete;

        //GETTERS FOR VULKAN HANDLES
        VkCommandPool getCommandPool() { return commandPool; }
        VkDevice device() { return device_; }
        VkSurfaceKHR surface() { return surface_; }
        VkQueue graphicsQueue() { return graphicsQueue_; }
        VkQueue presentQueue() { return presentQueue_; }

        // Returns swap chain support details for current physical device
        SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }

        //finds appropriate memory type index for buffer/image alloc
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        // Returns queue family indices for current physical device
        QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }

        // Finds supported format from candidates list based on tiling and features
        VkFormat findSupportedFormat(
                const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        //BUFFER HELPER FUNCTIONS

        // Creates buffer with specified size, usage, and memory properties
        void createBuffer(
                VkDeviceSize size,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags properties,
                VkBuffer &buffer,
                VkDeviceMemory &bufferMemory);

        // Begins single-use command buffer for one-time operations
        VkCommandBuffer beginSingleTimeCommands();

        // Ends and submits single-use command buffer
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        // Copies data between two buffers
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        // Copies buffer data to image (for texture loading)
        void copyBufferToImage(
                VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        // Creates image with specified properties and allocates memory
        void createImageWithInfo(
                const VkImageCreateInfo &imageInfo,
                VkMemoryPropertyFlags properties,
                VkImage &image,
                VkDeviceMemory &imageMemory);

        VkPhysicalDeviceProperties properties;  // Physical device properties (name, limits, etc.)

    private:
        //INITIALIZATION METHODs
        void createInstance();        // Creates Vulkan instance
        void setupDebugMessenger();   // Sets up validation layer debugging
        void createSurface();         // Creates window surface for rendering
        void pickPhysicalDevice();    // Selects suitable GPU
        void createLogicalDevice();   // Creates logical device with queues
        void createCommandPool();     // Creates command pool for command buffers

        //HELPER METHODS
        bool isDeviceSuitable(VkPhysicalDevice device);  // Checks if GPU meets requirements
        std::vector<const char *> getRequiredExtensions();  // Gets required instance extensions
        bool checkValidationLayerSupport();  // Verifies validation layers available
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);  // Finds queue families
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);  // Sets up debug info
        void hasGflwRequiredInstanceExtensions();  // Validates GLFW extensions available
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);  // Checks device extensions
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);  // Queries swap chain support

        //VULKAN HANDLES
        VkInstance instance;                    // Vulkan instance
        VkDebugUtilsMessengerEXT debugMessenger;  // Debug messenger for validation
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;  // Selected GPU
        LveWindow &window;                      // Reference to window wrapper
        VkCommandPool commandPool;              // Command pool for allocating command buffers

        VkDevice device_;           // Logical device
        VkSurfaceKHR surface_;      // Window surface for presenting
        VkQueue graphicsQueue_;     // Queue for graphics operations
        VkQueue presentQueue_;      // Queue for presentation operations

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};  // Validation layer
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};  // Swap chain extension
    };

}  // namespace lve