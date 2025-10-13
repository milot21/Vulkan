#include "lve_swap_chain.hpp"

// std
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
/**
 * swapchain holds the images and are queued for display
 * manages entire lifecycle of the swapchain from creating images, views
 * framebuffers, handling depth, render passes, sync, and presenting each frame
 *
 * */
namespace lve {

    /**
     * constructor: creates a new swap shain from scratch with given device + and window size
     * calls init to start build
     * */
    LveSwapChain::LveSwapChain(LveDevice &deviceRef, VkExtent2D extent)
            : device{deviceRef}, windowExtent{extent} {
        init();
    }

    /**
     * same as above but accepts previous chain
     * */
    LveSwapChain::LveSwapChain(LveDevice &deviceRef, VkExtent2D extent, std::shared_ptr<LveSwapChain> previous)
            : device{deviceRef}, windowExtent{extent}, oldSwapChain{previous} {
        init();
        oldSwapChain = nullptr;
    }

    /**
     * setup function
     * */
    void LveSwapChain::init() {
        createSwapChain(); // make the swap chain
        createImageViews(); // wrap swap chain images into image views
        createRenderPass(); // tells how rendering to images works
        createDepthResources();
        createFramebuffers(); //double or triple buffering enables concurrent data updates
        createSyncObjects(); //make semaphores/fences for cpu-gpu sync
    }

    /**
     * desturctor for all function calls
     * */
    LveSwapChain::~LveSwapChain() {
        for (auto imageView: swapChainImageViews) {
            vkDestroyImageView(device.device(), imageView, nullptr);
        }
        swapChainImageViews.clear();

        if (swapChain != nullptr) {
            vkDestroySwapchainKHR(device.device(), swapChain, nullptr);
            swapChain = nullptr;
        }

        for (int i = 0; i < depthImages.size(); i++) {
            vkDestroyImageView(device.device(), depthImageViews[i], nullptr);
            vkDestroyImage(device.device(), depthImages[i], nullptr);
            vkFreeMemory(device.device(), depthImageMemorys[i], nullptr);
        }

        for (auto framebuffer: swapChainFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }

        vkDestroyRenderPass(device.device(), renderPass, nullptr);

        // cleanup synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device.device(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device.device(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device.device(), inFlightFences[i], nullptr);
        }
    }

    /**
     * waits until the frame is free
     * asks for the next available swap chain image(imageAcquireImageKHR) thats being held in the queue
     * */
    VkResult LveSwapChain::acquireNextImage(uint32_t *imageIndex) {
        vkWaitForFences(
                device.device(),
                1,
                &inFlightFences[currentFrame],
                VK_TRUE,
                std::numeric_limits<uint64_t>::max()); // cpu will wait here

        VkResult result = vkAcquireNextImageKHR(
                device.device(),
                swapChain,
                std::numeric_limits<uint64_t>::max(),
                imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
                VK_NULL_HANDLE,
                imageIndex);

        return result;
    }

    /**
     * sumbits recorded gpu commands to the graphic queue
     * wiats on imageAvailableSemaphore
     * signals renderFInishedSemaphore when complete
     * presents finsihed image tothe swap cahinwith vkQueuePresentKHR
     * rotates currentFrame if we have double triple buffer
     * */
    VkResult LveSwapChain::submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex) {
        //WAIT FOR IMAGE TO BE AVAILABLE
        // Check if this swap chain image is still being used by another frame
        if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
            // Wait for the previous frame using this image to complete
            vkWaitForFences(device.device(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
        }
        // Mark this image as being used by current frame
        imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

        // SETUP COMMAND BUFFER SUBMISSION
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // Wait for image acquisition before starting color output
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};  // Wait before writing color
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;        // Semaphores to wait for
        submitInfo.pWaitDstStageMask = waitStages;          // Pipeline stages to wait at

        // Command buffers to execute
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = buffers;               // The recorded command buffer

        // Signal when rendering is complete
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;    // Signal when GPU finishes rendering

        // SUBMIT TO GPU
        // Reset fence before submission (fence starts signaled, needs reset)
        vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);

        // Submit command buffer to graphics queue with fence for CPU synchronization
        if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        //PRESENT TO SCREEN
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        // Wait for rendering to finish before presenting
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;     // Wait for render completion

        // Specify which swap chain and image to present
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;               // Swap chain to present to
        presentInfo.pImageIndices = imageIndex;             // Which image in swap chain to present

        // Submit presentation request to present queue
        auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

        //ADVANCE TO NEXT FRAME
        // Move to next frame in flight (circular buffer: 0, 1, 2, 0, 1, 2...)
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        return result;  // Return present result (may indicate swap chain needs recreation)
    }

    /**
     * chooses (chooseSwapSurfaceFormat, chooseSwapPresentMode, chooseSwapExtent)
     * then creates the swap chain with those parameters
     * retrieves the actual swap chain images
     * */
    void LveSwapChain::createSwapChain() {
        // Get device capabilities for swap chain creation
        SwapChainSupportDetails swapChainSupport = device.getSwapChainSupport();

        // Choose optimal settings from available options
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);  // Color format + color space
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);   // Presentation timing (vsync, etc.)
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);                   // Image dimensions

        // DETERMINE IMAGE COUNT
        // Request one more than minimum for better performance (triple buffering)
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        // Ensure we don't exceed maximum (0 means no limit)
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        //SETUP SWAP CHAIN CREATION INFO
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = device.surface();                      // Window surface to present to

        createInfo.minImageCount = imageCount;                      // Number of images in swap chain
        createInfo.imageFormat = surfaceFormat.format;              // Image format (B8G8R8A8_SRGB, etc.)
        createInfo.imageColorSpace = surfaceFormat.colorSpace;      // Color space (sRGB, etc.)
        createInfo.imageExtent = extent;                            // Image dimensions
        createInfo.imageArrayLayers = 1;                            // Single layer (not stereo)
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // Images used as color render targets

        // HANDLE QUEUE FAMILY SHARINg
        QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

        // Check if graphics and present queues are different families
        if (indices.graphicsFamily != indices.presentFamily) {
            // Concurrent: Both queue families can access images simultaneously
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            // Exclusive: Only one queue family owns images at a time (better performance)
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;      // Not needed for exclusive mode
            createInfo.pQueueFamilyIndices = nullptr;  // Not needed for exclusive mode
        }

        // ADDITIONAL SWAP CHAIN SETTINGS
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;  // No rotation/flip transformation
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;            // No alpha blending with other windows
        createInfo.presentMode = presentMode;                                     // Chosen presentation mode
        createInfo.clipped = VK_TRUE;                                             // Don't render obscured pixels

        // Link to old swap chain if recreating (for window resize, etc.)
        createInfo.oldSwapchain = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;

        //CREATE THE SWAP CHAIN
        if (vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // RETRIEVE SWAP CHAIN IMAGES
        // Implementation may create more images than requested minimum
        // First call gets actual count, second call gets the image handles
        vkGetSwapchainImagesKHR(device.device(), swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);  // Resize container for actual count
        vkGetSwapchainImagesKHR(device.device(), swapChain, &imageCount, swapChainImages.data());

        // Store selected format and extent for later use
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }


    /**
     * each swapchain image, creatres an image view so shaders/pipelines can use
     * */
    void LveSwapChain::createImageViews() {
                // Resize to match number of swap chain images (usually 2-3)
                swapChainImageViews.resize(swapChainImages.size());

                // Create one image view for each swap chain image
                for (size_t i = 0; i < swapChainImages.size(); i++) {
                    // Setup image view creation info
                    VkImageViewCreateInfo viewInfo{};
                    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    viewInfo.image = swapChainImages[i];                    // Raw swap chain image to create view for
                    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;              // Treat as 2D texture (not 1D, 3D, or cube)
                    viewInfo.format = swapChainImageFormat;                 // Same format as swap chain (B8G8R8A8_SRGB, etc.)

                    // Define which part of the image to access
                    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // Access color data (not depth/stencil)
                    viewInfo.subresourceRange.baseMipLevel = 0;             // Start at mip level 0 (full resolution)
                    viewInfo.subresourceRange.levelCount = 1;               // Only access 1 mip level (swap chain has no mipmaps)
                    viewInfo.subresourceRange.baseArrayLayer = 0;           // Start at array layer 0 (first layer)
                    viewInfo.subresourceRange.layerCount = 1;               // Only access 1 layer (swap chain is single layer)

                    // Create the image view
                    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &swapChainImageViews[i]) !=
                        VK_SUCCESS) {
                        throw std::runtime_error("failed to create texture image view!");
                    }
                }
            }

    /**
     * colorattachment, depth attachment, subpass, and information for rendering the image
     * */
    void LveSwapChain::createRenderPass() {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();                                      // Depth buffer format (D32_SFLOAT, etc.)
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                                 // No multisampling
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                            // Clear depth buffer at start
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;                      // Don't need depth after rendering
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                 // Don't care about stencil load
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;               // Don't care about stencil store
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                       // Don't care about initial state
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // Optimized for depth testing

        // Reference to depth attachment in subpass
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;                                       // Index 1 in attachments array
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // Layout during subpass

        // === COLOR ATTACHMENT DESCRIPTION ===
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = getSwapChainImageFormat();                      // Swap chain format (B8G8R8A8_SRGB, etc.)
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                         // No multisampling
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                    // Clear color buffer at start
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                  // Keep color data for presentation
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;       // No stencil in color buffer
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;         // No stencil in color buffer
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;               // Don't care about initial state
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;           // Ready for presentation to screen

        // Reference to color attachment in subpass
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;                                       // Index 0 in attachments array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    // Layout during subpass

        // === SUBPASS DESCRIPTION ===
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;             // Graphics pipeline (not compute)
        subpass.colorAttachmentCount = 1;                                        // One color attachment
        subpass.pColorAttachments = &colorAttachmentRef;                         // Color attachment reference
        subpass.pDepthStencilAttachment = &depthAttachmentRef;                   // Depth attachment reference

        // === SUBPASS DEPENDENCY ===
        // Ensures proper synchronization between external operations and our subpass
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;                             // External operations (swap chain)
        dependency.dstSubpass = 0;                                               // Our subpass (index 0)

        // Source: Wait for color/depth output stages to complete
        dependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                  // Color output stage
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;                     // Early depth testing
        dependency.srcAccessMask = 0;                                            // No specific memory access to wait for

        // Destination: Block color/depth writes until source completes
        dependency.dstStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                  // Our color output
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;                     // Our depth testing
        dependency.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |                           // Writing to color buffer
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;                    // Writing to depth buffer

        // === RENDER PASS CREATION ===
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());  // 2 attachments
        renderPassInfo.pAttachments = attachments.data();                       // Color + depth descriptions
        renderPassInfo.subpassCount = 1;                                         // Single subpass
        renderPassInfo.pSubpasses = &subpass;                                    // Subpass description
        renderPassInfo.dependencyCount = 1;                                      // One dependency
        renderPassInfo.pDependencies = &dependency;                              // Synchronization dependency

        // Create the render pass object
        if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    /**
     * for each image in the swap chain it creates a framebuffer
     * bings color+depth image views together for rendering
    * */
    void LveSwapChain::createFramebuffers() {
        // Resize to match number of swap chain images (usually 2-3 for double/triple buffering)
        swapChainFramebuffers.resize(imageCount());

        // Create one framebuffer for each swap chain image
        for (size_t i = 0; i < imageCount(); i++) {
            // Combine color and depth attachments for this framebuffer
            std::array<VkImageView, 2> attachments = {
                    swapChainImageViews[i],  // Color attachment (swap chain image for display)
                    depthImageViews[i]       // Depth attachment (for 3D depth testing)
            };

            // Get swap chain dimensions to size framebuffer
            VkExtent2D swapChainExtent = getSwapChainExtent();

            // Setup framebuffer creation info
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;                               // Compatible render pass
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());  // 2 attachments (color + depth)
            framebufferInfo.pAttachments = attachments.data();                     // Array of image views
            framebufferInfo.width = swapChainExtent.width;                         // Framebuffer width
            framebufferInfo.height = swapChainExtent.height;                       // Framebuffer height
            framebufferInfo.layers = 1;                                            // Single layer (not array texture)

            // Create the framebuffer object
            if (vkCreateFramebuffer(
                    device.device(),
                    &framebufferInfo,
                    nullptr,
                    &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    /**
     * creates a depth image for each image and allocates gpy memory to make it with the info
     * */
    void LveSwapChain::createDepthResources() {
        // Find best supported depth format (usually D32_SFLOAT or D24_UNORM_S8_UINT)
        VkFormat depthFormat = findDepthFormat();
        swapChainDepthFormat = depthFormat;  // Store format for later use

        // Get swap chain dimensions to match depth buffer size
        VkExtent2D swapChainExtent = getSwapChainExtent();

        // Resize arrays to match number of swap chain images (usually 2-3)
        depthImages.resize(imageCount());
        depthImageMemorys.resize(imageCount());
        depthImageViews.resize(imageCount());

        // Create one depth buffer per swap chain image
        for (int i = 0; i < depthImages.size(); i++) {
            // Setup depth image creation info
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;           // 2D texture

            // Match swap chain dimensions exactly
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;                       // 2D image has depth of 1

            imageInfo.mipLevels = 1;                          // No mipmapping for depth buffer
            imageInfo.arrayLayers = 1;                        // Single layer
            imageInfo.format = depthFormat;                   // Depth format (not color)
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;       // GPU-optimal memory layout
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // Don't care about initial contents
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // Used as depth attachment
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;        // No multisampling
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Single queue family access
            imageInfo.flags = 0;                              // No special flags

            // Create depth image and allocate GPU memory for it
            device.createImageWithInfo(
                    imageInfo,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,      // Store in fast GPU memory
                    depthImages[i],
                    depthImageMemorys[i]);

            // Create image view to access the depth image in shaders/render passes
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = depthImages[i];                  // Image to create view for
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;        // 2D image view
            viewInfo.format = depthFormat;                    // Same format as image

            // Specify which part of image to access
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  // Only depth data (not stencil)
            viewInfo.subresourceRange.baseMipLevel = 0;       // Use base mip level
            viewInfo.subresourceRange.levelCount = 1;         // Only one mip level
            viewInfo.subresourceRange.baseArrayLayer = 0;     // First array layer
            viewInfo.subresourceRange.layerCount = 1;         // Only one layer

            // Create the image view
            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create texture image view!");
            }
        }
    }

    /**
     * creates a semaphore for each frame
     * signals when image is ready to render -> imageAvailableSemaphore
     * signal when done rendering -> imageFinishedSemaphore
     * creates fences inFlightFences to sync
     *
     * */
    void LveSwapChain::createSyncObjects() {
        // Resize arrays to support MAX_FRAMES_IN_FLIGHT concurrent frames (usually 2-3)
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // Signals when swap chain image is ready
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // Signals when rendering is complete
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);            // CPU waits for GPU frame completion
        imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);    // Tracks which frame is using each image

        // Setup semaphore creation info (GPU-GPU synchronization)
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;  // No additional flags needed

        // Setup fence creation info (CPU-GPU synchronization)
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled so first frame doesn't wait

        // Create synchronization objects for each frame in flight
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            // Create semaphore to wait for image acquisition from swap chain
            // Create semaphore to signal when rendering to image is finished
            // Create fence for CPU to wait until GPU finishes this frame
            if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateFence(device.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    /**
     * picks best color format that is available
     * */
    VkSurfaceFormatKHR LveSwapChain::chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat: availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    /**
     * present mode determines whens and how the images are swapped to the screen,
     * first checks mailbox(best smoothness and no tearing)
     * if not available then it chooses FIFO(safe, supported. vsync)
     * */
    VkPresentModeKHR LveSwapChain::chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode: availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                std::cout << "Present mode: Mailbox" << std::endl;
                return availablePresentMode;
            }
        }

        // for (const auto &availablePresentMode : availablePresentModes) {
        //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        //     std::cout << "Present mode: Immediate" << std::endl;
        //     return availablePresentMode;
        //   }
        // }

        std::cout << "Present mode: V-Sync" << std::endl;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * chooses resolution of the swap chain images
     * if currentExtent is fixed by the surfac it uses it otherwise window size
     * */
    VkExtent2D LveSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent = windowExtent;
            actualExtent.width = std::max(
                    capabilities.minImageExtent.width,
                    std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(
                    capabilities.minImageExtent.height,
                    std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }


    /**
     * finds a supported format for the depth buffer
     * D32_SFLOAT,D32_SFLOAT_S8_UINT, D24_UNORM_S8_UINT
     * */
    VkFormat LveSwapChain::findDepthFormat() {
        return device.findSupportedFormat(
                {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

}  // namespace lve
