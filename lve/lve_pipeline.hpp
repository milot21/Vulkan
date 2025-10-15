//
// Created by cdgira on 7/6/2023.
//

#ifndef VULKANTEST_LVE_PIPELINE_HPP
#define VULKANTEST_LVE_PIPELINE_HPP

#include "lve_device.hpp"

#include <string>
#include <vector>

namespace lve {

    //helper struct that stores all vulkan fixed function pipeline configs
    struct PipelineConfigInfo {

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    /**
     * LvePipeline: a wrapper around a Vulkan graphics pipeline obejct
     * load compiled shader, config pipeline state, create and manage pipeline
     * bing the pipeline to cmd buffers for rendering
     * */
    class LvePipeline {

    public:
        /**
         * Constructor: builds a graphics pipeline
         * @param device        - refrence to the lveDevice
         * @param vertFilepath  - path to compiled vertex shader
         * @param fragFilepath  - path to compiled fragment shader
         * @param configInfo    -fixed-function pipeline state config
         * */
        LvePipeline(LveDevice &device, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
        ~LvePipeline(); //destructor
        LvePipeline(const LvePipeline&) = delete;
        LvePipeline &operator=(const LvePipeline&) = delete;

        /**
         * binds the pipeline to a given command buffer
         * */
        void bind(VkCommandBuffer commandBuffer);

        static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);


    private:
        //reads binary SPIR-V shader file into memory
        static std::vector<char> readFile(const std::string &filename);
        // creates the vulkan graphics pipeline
        void createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo);
        //creates a vkShaderModule from SPIR-v bytecode
        void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

        LveDevice &lveDevice; //reference to the logical device wrapper
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule; //compiled vertex shader module
        VkShaderModule fragShaderModule; // compiled frag shader module

    };

}


#endif //VULKANTEST_LVE_PIPELINE_HPP
