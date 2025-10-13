//
// Created by milot on 9/18/2025.
//
#pragma once

#include "lve_device.hpp"
#include "lve_swap_chain.hpp"
#include "lve_window.hpp"

// std
#include <cassert>
#include <memory>
#include <vector>

namespace lve {
    /**
     * Handles the rendering pipeline and frame management for Vulkan
     * Manages the swapchain, cmd buffers, and render passes
     * handles window resizes
     */
    class LveRenderer {
    public:
        /**
         * Init renderer with window and device
         * creates initial swap chain and command buffers
         * @param window refrence to the GLFW window wrapper
         * @param device vulkans device wrapper
         */
        LveRenderer(LveWindow &window, LveDevice &device);
        ~LveRenderer();//destructor, cleans cmd buff and swapchain

        LveRenderer(const LveRenderer &) = delete;
        LveRenderer &operator=(const LveRenderer &) = delete;

        /**
         * gets render pass from current swap chain
         * used by render systems to create compaitble pipelines
         * @return VkRenderPass handle for the swap chains render pass
         */
        VkRenderPass getSwapChainRenderPass() const { return lveSwapChain->getRenderPass(); }
        //gets aspect ration of the swap chain images
        //is used for camera projection matrix calc
        float getAspectRatio() const {return lveSwapChain->extentAspectRatio(); }
        bool isFrameInProgress() const { return isFrameStarted; } //checks and is used to precent invalid state transitions

        //Gets cmd buff for current frame, each frame has it's own
        VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }

        /**
         * gets the indec of the current frame being rendered
         * used for accessing frame-specific resources (uniforms, descriptors)
         * @return
         */
        int getFrameIndex() const {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            return currentFrameIndex;
        }


        VkCommandBuffer beginFrame(); // new frame, acquires swapchain and starts cmd buff recording
        void endFrame(); //ends frame and presents to screen
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    private:
        void createCommandBuffers(); //create cmd buff for each frame in flight
        void freeCommandBuffers(); //free allocated buffers
        void recreateSwapChain(); // recreates a swap chain when necessary

        LveWindow &lveWindow;                       // refrence to window manager
        LveDevice &lveDevice;                       // refrence to vulkan device wrapper
        std::unique_ptr<LveSwapChain> lveSwapChain; // managed swap chain instances
        std::vector<VkCommandBuffer> commandBuffers;// one cmd buff per frame

        uint32_t currentImageIndex;                 //index of current swap chain image
        int currentFrameIndex;                      // current frame index
        bool isFrameStarted;                        //state flag to prevent incalid ops
    };
}  // namespace lve