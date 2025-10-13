//
// Created by cdgira on 6/30/2023.
//

#ifndef VULKANTEST_LVE_WINDOW_HPP
#define VULKANTEST_LVE_WINDOW_HPP
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace lve {

    class LveWindow {

    public:
        LveWindow(int w, int h, std::string name); // init member variables
        ~LveWindow(); // destructor for the window

        LveWindow(const LveWindow&) = delete;
        LveWindow &operator=(const LveWindow&) = delete;

        bool shouldClose() { return glfwWindowShouldClose(window); } // quarying wether or not the user tried to dismiss the window
        VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
        bool wasWindowResized() { return framebufferResized; }
        void resetWindowResizedFlag() { framebufferResized = false; }
        GLFWwindow *getGLFWwindow() const {return window; }

        void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

    private:
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
        void initWindow(); //window helper function

        int width;
        int height;
        bool framebufferResized = false;

        std::string windowName;
        GLFWwindow* window; //pointer to a glfw window
    };
}

#endif //VULKANTEST_LVE_WINDOW_HPP
