//
// Created by milot on 9/18/2025.
//

#include "lve_renderer.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

    /**
     * Constructor: Init renderer with window and device refrences
     * @param window
     * @param device
     */
    LveRenderer::LveRenderer(LveWindow& window, LveDevice& device)
            : lveWindow{window}, lveDevice{device} {
        recreateSwapChain(); //create init swap chain
        createCommandBuffers(); //allocate cmd buff for each frame
    }

    //Destructor
    LveRenderer::~LveRenderer() { freeCommandBuffers(); }

    /**
     * recreates the swap chain when window is resized or fromat changes
     * handles window minim by waiting for valid dimensions
     */
    void LveRenderer::recreateSwapChain() {
        auto extent = lveWindow.getExtent();   // get current window dim
        //wait while window is minimized
        while (extent.width == 0 || extent.height == 0) {
            extent = lveWindow.getExtent();
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(lveDevice.device()); //wait for all GPU ops to complete before recreating

        //create new swapchain or recreate existing one
        if (lveSwapChain == nullptr) {
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent); //new
        } else {
            std::shared_ptr<LveSwapChain> oldSwapChain = std::move(lveSwapChain); // old reusing
            lveSwapChain = std::make_unique<LveSwapChain>(lveDevice, extent, oldSwapChain);

            if (!oldSwapChain->compareSwapFormats(*lveSwapChain.get())) {
                throw std::runtime_error("Swap chain image(or depth) format has changed!");
            }
        }
    }

    /**
     * Creates cmd buff for each frame in flight
     * uses primary cmd buff that can execute other cmd buffs
     */
    void LveRenderer::createCommandBuffers() {
        commandBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT); //resize

        //set up allocation parameters
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = lveDevice.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        //allocate buffers at once
        if (vkAllocateCommandBuffers(lveDevice.device(), &allocInfo, commandBuffers.data()) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    /**
    * frees all allocated command buffers
    * called during destruction and before swap chain recreation
    * @return
    */
    void LveRenderer::freeCommandBuffers() {
        vkFreeCommandBuffers(
                lveDevice.device(),
                lveDevice.getCommandPool(),
                static_cast<uint32_t>(commandBuffers.size()),
                commandBuffers.data());
        commandBuffers.clear();
    }

    /**
     * begins new frame for rendering
     * acquires next avail swapchain and prepares cmd buff
     * @return
     */
    VkCommandBuffer LveRenderer::beginFrame() {
        assert(!isFrameStarted && "Can't call beginFrame while already in progress");

        //try to acquire the next image from swapchain
        auto result = lveSwapChain->acquireNextImage(&currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return nullptr;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        isFrameStarted = true; // mark frame in progress

        // get the cmd buffer for this frame and start recording
        auto commandBuffer = getCurrentCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }
        return commandBuffer;
    }

    /**
     * ends the current frame and submits it for representation
     */
    void LveRenderer::endFrame() {
        assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
        auto commandBuffer = getCurrentCommandBuffer();
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

        //sumbit cmd buffer and present the rendered image
        auto result = lveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            lveWindow.wasWindowResized()) {
            lveWindow.resetWindowResizedFlag();
            recreateSwapChain();  //recreate the swapchain if out of date
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        isFrameStarted = false;  //mark complete
        //advance to next frame
        currentFrameIndex = (currentFrameIndex + 1) % LveSwapChain::MAX_FRAMES_IN_FLIGHT;
    }

    /**
     * begins swap chain render pass with standard setup
     * configs viewport, scissor rectangle and clear values
     * @param commandBuffer
     */
    void LveRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
        assert(
                commandBuffer == getCurrentCommandBuffer() &&
                "Can't begin render pass on command buffer from a different frame");

        //set up render pass begin info
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = lveSwapChain->getRenderPass();  //render pass to use
        renderPassInfo.framebuffer = lveSwapChain->getFrameBuffer(currentImageIndex);  //Traget framebuffer

        //set render area to full swapchain extent
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = lveSwapChain->getSwapChainExtent();

        //Configure clear values for color and depgth buffers
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f}; // Dark grey background
        clearValues[1].depthStencil = {1.0f, 0};                                   //Clear depth to far plane, stencil to 0
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // begin render pass
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // setup view port transformation
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(lveSwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(lveSwapChain->getSwapChainExtent().height);
        viewport.minDepth = 0.0f; //Near plane
        viewport.maxDepth = 1.0f; // Far plane
        //setup scissor rectangle
        VkRect2D scissor{{0, 0}, lveSwapChain->getSwapChainExtent()};
        //apply viewport and scissor to cmd buffer
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    /**
     * ends current swapchain render pass
     *
     * @param commandBuffer
     */
    void LveRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress");
        assert(
                commandBuffer == getCurrentCommandBuffer() &&
                "Can't end render pass on command buffer from a different frame");
        vkCmdEndRenderPass(commandBuffer);
    }

}  // namespace lve