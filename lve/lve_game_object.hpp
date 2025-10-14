//
// Created by milot on 9/13/2025.
//
#pragma once

#include "lve_model.hpp"
#include <glm/glm.hpp>
//libs
#include <glm/gtc/matrix_transform.hpp>
// std
#include <memory>

namespace lve {

    /**
     * Handles 3d transformation data and matrix calc
     * translation: position in 3d space
     * Scale: sizze along the x, y, z axes
     * rotation: Euler angles for orientation
     */
    struct TransformComponent {
        glm::vec3 translation{};  // (position offset)
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{};

        // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
        // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
        // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
        glm::mat4 mat4();
        glm::mat3 normalMatrix();
    };

    /**
     * represents one entity in the game world, and it's properties
     * Unique ID for each object
     * shared pointer to a 3d model
     * RGB color for the object
     * transform position, rotation, scale
     */
    class LveGameObject {
    public:
        using id_t = unsigned int; //object ID

        //Factory method
        //uses static counter to ensure unique identifiers
        static LveGameObject createGameObject() {
            static id_t currentId = 0;
            return LveGameObject{currentId++};
        }

        //disable copying to prevent issues with unique IDs
        LveGameObject(const LveGameObject &) = delete;
        LveGameObject &operator=(const LveGameObject &) = delete;
        LveGameObject(LveGameObject &&) = default;
        LveGameObject &operator=(LveGameObject &&) = default;

        id_t getId() { return id; }

        //easy access during rendering
        std::shared_ptr<LveModel> model{}; //shared pointer to 3d model
        glm::vec3 color{};                 // RBG color values
        TransformComponent transform{};    //position, rotation, scale data

    private:
        //private constructor to enforce use of factory method
        //proper id managment
        LveGameObject(id_t objId) : id{objId} {}

        id_t id;
    };
}  // namespace lve
