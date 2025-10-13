//
// Created by cdgira on 6/30/2023.
//

#include "lve_window.hpp"

#include <stdexcept>

namespace lve {
         // constructor
        LveWindow::LveWindow(int w, int h, std::string name) : width(w), height(h), windowName(name) {
            initWindow();
        }

        //constructor destroyer
        LveWindow::~LveWindow() {
            glfwDestroyWindow(window);
            glfwTerminate();
        }


        void LveWindow::initWindow() {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //not to create a glfw context
            glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); //disable resize after creation

            window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        }

        void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
            if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
                throw std::runtime_error("failed to create window surface!");
            }
        }

        void LveWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
            auto lveWindow = reinterpret_cast<LveWindow*>(glfwGetWindowUserPointer(window));
            lveWindow->framebufferResized = true;
            lveWindow->width = width;
            lveWindow->height = height;
        }


}
