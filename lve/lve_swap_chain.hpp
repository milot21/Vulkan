#pragma once

#include "lve_device.hpp"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <memory>
#include <string>
#include <vector>

namespace lve {

    class LveSwapChain {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // Number of frames processed simultaneously (double buffering)

        // Constructor: Creates new swap chain with specified window dimensions
        LveSwapChain(LveDevice &deviceRef, VkExtent2D windowExtent);

        // Constructor: Creates new swap chain while keeping reference to old one (for recreation)
        LveSwapChain(LveDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<LveSwapChain> previous);

        // Destructor: Cleans up all swap chain resources
        ~LveSwapChain();

        // Prevent copying/moving to avoid double cleanup of Vulkan resources
        LveSwapChain(const LveSwapChain &) = delete;
        LveSwapChain &operator=(const LveSwapChain &) = delete;

        //GETTERS FOR SWAP CHAIN RESOURCES
        VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }  // Get framebuffer by index
        VkRenderPass getRenderPass() { return renderPass; }                              // Get render pass for drawing
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }       // Get image view by index
        size_t imageCount() { return swapChainImages.size(); }                           // Number of swap chain images
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }              // Color format of images
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }                      // Dimensions of images
        uint32_t width() { return swapChainExtent.width; }                               // Image width in pixels
        uint32_t height() { return swapChainExtent.height; }                             // Image height in pixels

        // Calculate aspect ratio for camera/projection setup
        float extentAspectRatio() {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }

        // Find best depth buffer format supported by device
        VkFormat findDepthFormat();

        // RENDERING OPERATIONS
        VkResult acquireNextImage(uint32_t *imageIndex);                                 // Get next available image for rendering
        VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);  // Submit and present frame

        // Compare formats between swap chains (used when recreating)
        bool compareSwapFormats(const LveSwapChain &swapChain) const {
            return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
                   swapChain.swapChainImageFormat == swapChainImageFormat;
        }

    private:
        // INITIALIZATION METHODS
        void init();                    // Initialize swap chain and all resources
        void createSwapChain();         // Create the swap chain object
        void createImageViews();        // Create views for swap chain images
        void createDepthResources();    // Create depth buffers
        void createRenderPass();        // Create render pass template
        void createFramebuffers();      // Create framebuffers combining color+depth
        void createSyncObjects();       // Create semaphores and fences

        // === HELPER METHODS FOR SWAP CHAIN CREATION ===
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(
                const std::vector<VkSurfaceFormatKHR> &availableFormats);          // Pick best color format
        VkPresentModeKHR chooseSwapPresentMode(
                const std::vector<VkPresentModeKHR> &availablePresentModes);       // Pick best present mode (vsync, etc.)
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);  // Pick best image dimensions

        // SWAP CHAIN PROPERTIES
        VkFormat swapChainImageFormat;    // Color format of swap chain images
        VkFormat swapChainDepthFormat;    // Depth format for depth buffers
        VkExtent2D swapChainExtent;       // Dimensions of swap chain images

        //RENDERING RESOURCES
        std::vector<VkFramebuffer> swapChainFramebuffers;  // Framebuffers for each swap chain image
        VkRenderPass renderPass;                           // Render pass template

        // DEPTH BUFFER RESOURCES
        std::vector<VkImage> depthImages;         // Depth buffer images
        std::vector<VkDeviceMemory> depthImageMemorys;  // Memory for depth images
        std::vector<VkImageView> depthImageViews; // Views for depth images

        //SWAP CHAIN IMAGES
        std::vector<VkImage> swapChainImages;      // Images provided by swap chain
        std::vector<VkImageView> swapChainImageViews;  // Views for swap chain images

        // DEVICE AND WINDOW REFERENCES
        LveDevice &device;              // Reference to Vulkan device wrapper
        VkExtent2D windowExtent;        // Window dimensions

        // SWAP CHAIN OBJECTS
        VkSwapchainKHR swapChain;                      // Main swap chain object
        std::shared_ptr<LveSwapChain> oldSwapChain;    // Previous swap chain (for recreation)

        // SYNCHRONIZATION OBJECTS
        std::vector<VkSemaphore> imageAvailableSemaphores;   // Signal when image is ready for rendering
        std::vector<VkSemaphore> renderFinishedSemaphores;   // Signal when rendering is complete
        std::vector<VkFence> inFlightFences;                 // CPU waits for GPU frame completion
        std::vector<VkFence> imagesInFlight;                 // Track which frame is using each image
        size_t currentFrame = 0;                             // Current frame index (0 to MAX_FRAMES_IN_FLIGHT-1)
    };

}  // namespace lve
