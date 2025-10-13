//
// Created by milot on 9/08/2025.
//

#ifndef VULKANTEST_FILEREADER_HPP
#define VULKANTEST_FILEREADER_HPP


#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "lve/lve_model.hpp"

namespace lve {

    class fileReader {
    public:
        struct PixelData {
            int width;
            int height;
            std::vector<std::vector<int>> pixels;
        };

        // load character data from file
        static PixelData loadCharacterFile(const std::string& filename);

        // to create vertices from pixel data
        static std::vector<LveModel::Vertex> createVerticesFromPixelData(
                const PixelData& pixelData,
                const std::unordered_map<int, glm::vec3>& colorPalette,
                float pixelSize = 0.02f
        );
    };
} // namespace lve


#endif //VULKANTEST_FILEREADER_HPP
