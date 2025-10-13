//
// Created by milot on 9/13/2025.
//
#pragma once
#include "lve/lve_camera.hpp"
#include "lve/lve_device.hpp"
#include "lve/lve_game_object.hpp"
#include "lve/lve_pipeline.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
    class SimpleRenderSystem {
    public:
        SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass);
        ~SimpleRenderSystem();

        SimpleRenderSystem(const SimpleRenderSystem &) = delete;
        SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

        void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<LveGameObject> &gameObjects, const LveCamera &camera);

    private:
        void createPipelineLayout();
        void createPipeline(VkRenderPass renderPass);

        LveDevice &lveDevice;

        std::unique_ptr<LvePipeline> lvePipeline;
        VkPipelineLayout pipelineLayout;
    };
}  // namespace lve