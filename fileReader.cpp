//
// Created by milot on 9/08/2025.
//
#include "fileReader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

namespace lve {

    /**
     * Param: filename
     * Takes in a file name and reads the dimensions of the picture
     * then it reads the pixel data that are in a matrix
     * */
    fileReader::PixelData fileReader::loadCharacterFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open character file: " + filename);
        }

        PixelData pixelData;
        std::string line;

        // Read dimensions
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            iss >> pixelData.width >> pixelData.height;
        } else {
            throw std::runtime_error("Couldn't read dimensions from file");
        }

        // Read pixel data
        pixelData.pixels.resize(pixelData.height);

        for (int row = 0; row < pixelData.height; row++) {
            if (!std::getline(file, line)) {
                throw std::runtime_error("Unexpected end of file while reading pixel data");
            }

            std::istringstream iss(line);
            pixelData.pixels[row].resize(pixelData.width);

            for (int col = 0; col < pixelData.width; col++) {
                if (!(iss >> pixelData.pixels[row][col])) {
                    throw std::runtime_error("Could not read pixel value at row " +
                                             std::to_string(row) + ", col " + std::to_string(col));
                }
            }
        }

        file.close();
        return pixelData;
    }

    /**
     *Param: pixelData, colorPalette, pixelSize
     * centering the character around 0,0
     * skip background color
     * get color from the palette
     * each pixel becomes a quad of two triangles
     * returns: vector of vertices that vulkan can render
     * */
    std::vector<LveModel::Vertex> fileReader::createVerticesFromPixelData(
            const PixelData& pixelData,
            const std::unordered_map<int, glm::vec3>& colorPalette,
            float pixelSize) {

        std::vector<LveModel::Vertex> vertices;

        // Calculate centering offset to center the character
        float totalWidth = pixelData.width * pixelSize;
        float totalHeight = pixelData.height * pixelSize;
        glm::vec2 centerOffset = {-totalWidth / 2.0f, -totalHeight / 2.0f};

        for (int row = 0; row < pixelData.height; row++) {
            for (int col = 0; col < pixelData.width; col++) {
                int colorIndex = pixelData.pixels[row][col];

                // skip transparent/background pixels (assuming 4 is background)
                if (colorIndex == 4) continue;

                // Get color from palette
                glm::vec3 color = {1.0f, 1.0f, 1.0f}; // Default white
                auto colorIt = colorPalette.find(colorIndex);
                if (colorIt != colorPalette.end()) {
                    color = colorIt->second;
                }

                // Calculate pixel position
                float x = col * pixelSize + centerOffset.x;
                float y = row * pixelSize + centerOffset.y;

                // triangle 1
                vertices.push_back({{x, y, 0.0f}, color});                           // top-left
                vertices.push_back({{x + pixelSize, y, 0.0f}, color});               // top-right
                vertices.push_back({{x, y + pixelSize, 0.0f}, color});               // bottom-left

                // triangle 2
                vertices.push_back({{x + pixelSize, y, 0.0f}, color});               // top-right
                vertices.push_back({{x + pixelSize, y + pixelSize, 0.0f}, color});   // bottom-right
                vertices.push_back({{x, y + pixelSize, 0.0f}, color});               // bottom-left
            }
        }
        return vertices;
    }
} // namespace lve