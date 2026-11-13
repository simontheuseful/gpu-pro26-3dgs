#pragma once

#include <GLFW/glfw3.h>

class Window
{
    public:
        GLFWwindow* getWindow() const { return window; }
        uint32_t getWidth() const { return WIDTH; }
        uint32_t getHeight() const { return HEIGHT; }


        void initWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        }


        void cleanup()
        {
            glfwDestroyWindow(window);

            glfwTerminate();
        }


    private:
        GLFWwindow *window = nullptr; 
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

};