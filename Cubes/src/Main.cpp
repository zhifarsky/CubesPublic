#pragma region includes
#include <iostream>
#include <windows.h>
#include <thread>

#include "Header.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#pragma endregion

void CubesMainGameLoop(GLFWwindow* window);

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
    GLFWwindow* window;
#pragma region GLFW
    if (!glfwInit())
        return -1;
    window = glfwCreateWindow(1280, 720, "CUBES", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // disable vsync
#pragma endregion

    gladLoadGL();

#pragma region Imgui
    const char* glsl_version = "#version 330";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImGui::StyleColorsDark();
#pragma endregion

    // MAIN LOOP
    CubesMainGameLoop(window);

    glfwTerminate();

    return 0;
}