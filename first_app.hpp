//
// Created by cdgira on 6/30/2023.
//

#ifndef VULKANTEST_FIRST_APP_HPP
#define VULKANTEST_FIRST_APP_HPP

#include "lve/lve_window.hpp"
#include "lve/lve_renderer.hpp"
#include "lve/lve_device.hpp"
#include "lve/lve_game_object.hpp"
#include "lve/lve_descriptor.hpp"

#include <memory>
#include <vector>

namespace lve {
    class FirstApp {

    public:
        static constexpr int WIDTH = 800; //window weight
        static constexpr int HEIGHT = 600;

        FirstApp();
        ~FirstApp();

        FirstApp(const FirstApp&) = delete;
        FirstApp &operator=(const FirstApp&) = delete;

        void run();

    private:
        void loadGameObjects();
       // void updateAnimations();


        LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        LveDevice lveDevice{lveWindow};
        LveRenderer lveRenderer{lveWindow, lveDevice};

        //order of declaration matteers
        std::unique_ptr<LveDescriptorPool> globalPool{};
        std::vector<LveGameObject> gameObjects;
    };
}

#endif //VULKANTEST_FIRST_APP_HPP
