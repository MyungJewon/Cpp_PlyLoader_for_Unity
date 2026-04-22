// PLY 포인트 클라우드 독립 실행 뷰어 (GLFW + OpenGL)
// 조작: 마우스 좌클릭 드래그 → 회전, 스크롤 → 확대/축소

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif

#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif
#include "PointCloud.h"
#include "PlyParser.h"

#ifdef _WIN32
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER  0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW   0x88B4
#endif
typedef ptrdiff_t GLsizeiptr;
typedef void (*PFNGLGENBUFFERSPROC)   (GLsizei n, GLuint* buffers);
typedef void (*PFNGLBINDBUFFERPROC)   (GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC)   (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
PFNGLGENBUFFERSPROC    glGenBuffers    = nullptr;
PFNGLBINDBUFFERPROC    glBindBuffer    = nullptr;
PFNGLBUFFERDATAPROC    glBufferData    = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
#endif

struct Camera {
    float zoom    = 1.0f;
    float angleX  = 0.0f;
    float angleY  = 0.0f;
    bool   isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};

Camera     g_camera;
PointCloud g_cloud;
GLuint     g_vbo = 0;

void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_camera.isDragging = true;
            glfwGetCursorPos(glfwGetCurrentContext(), &g_camera.lastMouseX, &g_camera.lastMouseY);
        }
        else if (action == GLFW_RELEASE) {
            g_camera.isDragging = false;
        }
    }
}

void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (g_camera.isDragging) {
        g_camera.angleY += (float)(xpos - g_camera.lastMouseX) * 0.5f;
        g_camera.angleX += (float)(ypos - g_camera.lastMouseY) * 0.5f;
        g_camera.lastMouseX = xpos;
        g_camera.lastMouseY = ypos;
    }
}

void scrollCallback(GLFWwindow*, double, double yoffset) {
    g_camera.zoom += (float)yoffset * 0.1f;
    if (g_camera.zoom < 0.1f) g_camera.zoom = 0.1f;
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)width / (float)height;
    float s = g_cloud.orthoSize;
    if (width >= height)
        glOrtho(-s * aspect, s * aspect, -s, s, -s * 2.0f, s * 2.0f);
    else
        glOrtho(-s, s, -s / aspect, s / aspect, -s * 2.0f, s * 2.0f);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::string filePath;
    if (argc >= 2) {
        filePath = argv[1];
    }
    else {
        std::cout << "PLY 파일 경로를 입력하세요: ";
        std::getline(std::cin, filePath);
        if (!filePath.empty() && filePath.front() == '"') filePath = filePath.substr(1);
        if (!filePath.empty() && filePath.back()  == '"') filePath.pop_back();
    }

    if (!loadPly(filePath, g_cloud)) return -1;

    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Point Cloud Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);

#ifdef _WIN32
    glGenBuffers    = reinterpret_cast<PFNGLGENBUFFERSPROC>   (glfwGetProcAddress("glGenBuffers"));
    glBindBuffer    = reinterpret_cast<PFNGLBINDBUFFERPROC>   (glfwGetProcAddress("glBindBuffer"));
    glBufferData    = reinterpret_cast<PFNGLBUFFERDATAPROC>   (glfwGetProcAddress("glBufferData"));
    glDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(glfwGetProcAddress("glDeleteBuffers"));
    bool vboAvailable = glGenBuffers && glBindBuffer && glBufferData && glDeleteBuffers;
    if (!vboAvailable)
        std::cout << "[경고] VBO를 지원하지 않는 환경입니다." << std::endl;
#else
    bool vboAvailable = true;
#endif

    if (vboAvailable) {
        glGenBuffers(1, &g_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, g_cloud.rawData.size(), g_cloud.rawData.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        std::cout << "VBO 생성 완료 (ID: " << g_vbo << ")" << std::endl;
    }

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window,     mouseButtonCallback);
    glfwSetCursorPosCallback(window,       cursorPosCallback);
    glfwSetScrollCallback(window,          scrollCallback);

    framebufferSizeCallback(window, 1280, 720);
    glPointSize(2.0f);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glScalef(g_camera.zoom,    g_camera.zoom,    g_camera.zoom);
        glRotatef(g_camera.angleX, 1.0f, 0.0f, 0.0f);
        glRotatef(g_camera.angleY, 0.0f, 1.0f, 0.0f);
        glTranslatef(-g_cloud.centerX, -g_cloud.centerY, -g_cloud.centerZ);

        glEnableClientState(GL_VERTEX_ARRAY);

        if (vboAvailable) {
            glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
            glVertexPointer(3, GL_FLOAT, g_cloud.stride, (const void*)0);
            if (g_cloud.colorOffset != -1) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(3, GL_UNSIGNED_BYTE, g_cloud.stride, (const void*)(uintptr_t)g_cloud.colorOffset);
            }
        }
        else {
            glVertexPointer(3, GL_FLOAT, g_cloud.stride, g_cloud.rawData.data());
            if (g_cloud.colorOffset != -1) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(3, GL_UNSIGNED_BYTE, g_cloud.stride, g_cloud.rawData.data() + g_cloud.colorOffset);
            }
        }

        glDrawArrays(GL_POINTS, 0, g_cloud.vertexCount);

        glDisableClientState(GL_VERTEX_ARRAY);
        if (g_cloud.colorOffset != -1) glDisableClientState(GL_COLOR_ARRAY);
        if (vboAvailable) glBindBuffer(GL_ARRAY_BUFFER, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (vboAvailable && g_vbo != 0) {
        glDeleteBuffers(1, &g_vbo);
        std::cout << "VBO 해제 완료" << std::endl;
    }

    glfwTerminate();
    return 0;
}
