#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstddef>   // ptrdiff_t (GLsizeiptr 정의에 필요)
#include <cstdint>   // uintptr_t (포인터 크기의 정수형, 오프셋 캐스팅에 필요)
#include <windows.h> // SetConsoleOutputCP (콘솔 한글 출력용)
#include "PointCloud.h"
#include "PlyParser.h"

// ============================================================
// VBO(Vertex Buffer Object)란?
// ============================================================
// 지금까지는 점 데이터(rawData)가 CPU RAM에 있었습니다.
// 그래서 매 프레임마다 CPU RAM → GPU로 데이터를 전송해야 했습니다.
// 마치 냉장고(RAM)에서 식재료를 꺼내 요리사(GPU)에게 매번 가져다주는 것과 같습니다.
//
// VBO는 데이터를 GPU 메모리(VRAM)에 미리 올려두는 "그릇"입니다.
// 한 번 올려두면 매 프레임마다 전송 없이 GPU가 직접 꺼내 씁니다.
// 요리사 옆 작업대에 식재료를 미리 올려두는 것과 같습니다.
//
// [왜 직접 함수 포인터를 로딩하나요?]
// Windows의 기본 OpenGL 헤더(gl.h)는 아주 오래된 버전(1.1)만 지원합니다.
// VBO는 OpenGL 1.5에서 추가된 기능이라서 함수가 헤더에 없습니다.
// 그래서 glfwGetProcAddress()로 그래픽 드라이버에서 함수를 직접 꺼내와야 합니다.
// (GLEW나 GLAD 같은 라이브러리가 이 작업을 자동화해주는 것입니다.)
// ============================================================

// VBO 관련 OpenGL 상수 (gl.h 1.1에는 없어서 직접 정의)
#define GL_ARRAY_BUFFER  0x8892  // "이건 정점 데이터용 버퍼입니다"라고 알려주는 타입
#define GL_STATIC_DRAW   0x88B4  // "이 데이터는 한 번 올리고 자주 읽기만 할 거예요"

// GLsizeiptr: 버퍼 크기를 나타내는 타입 (포인터와 같은 크기의 정수)
// 64비트 환경에서는 8바이트, 32비트에서는 4바이트
// Windows gl.h 1.1에 없어서 직접 정의합니다.
typedef ptrdiff_t GLsizeiptr;

// VBO 함수 포인터 타입 정의
// typedef void (*이름)(파라미터) 형태로 "함수를 가리키는 포인터의 타입"을 만듭니다.
// 일반 변수가 값을 담듯이, 함수 포인터는 함수의 주소를 담습니다.
typedef void (*PFNGLGENBUFFERSPROC)   (GLsizei n, GLuint* buffers);
typedef void (*PFNGLBINDBUFFERPROC)   (GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC)   (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);

// 실제 함수 포인터 변수 (처음엔 비어있고, 나중에 드라이버에서 로딩)
PFNGLGENBUFFERSPROC    glGenBuffers    = nullptr;
PFNGLBINDBUFFERPROC    glBindBuffer    = nullptr;
PFNGLBUFFERDATAPROC    glBufferData    = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;


// ============================================================
// 카메라 상태 구조체
// ============================================================
struct Camera {
    float zoom    = 1.0f;  // 확대/축소 배율 (스크롤로 조절)
    float angleX  = 0.0f;  // X축 회전 각도 (마우스 상하 드래그)
    float angleY  = 0.0f;  // Y축 회전 각도 (마우스 좌우 드래그)

    bool   isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};


// ============================================================
// 전역 인스턴스
// ============================================================
Camera     g_camera;
PointCloud g_cloud;
GLuint     g_vbo = 0; // VBO의 ID (유니티의 instanceID 같은 개념)
            // OpenGL은 GPU에 만든 버퍼를 숫자 ID로 관리합니다.
            // 0은 "아직 아무것도 없음"을 의미합니다.


// ============================================================
// GLFW 콜백 함수들
// ============================================================

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

    // 콘솔 출력을 UTF-8로 설정 (한글 깨짐 방지)
    // Windows 터미널은 기본이 CP949(한국어 인코딩)라서
    // UTF-8로 작성된 한글 문자열이 깨져 보입니다.
    // 65001 = UTF-8 코드 페이지 번호
    SetConsoleOutputCP(65001);

    // --- 파일 경로 결정 ---
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

    // --- PLY 파일 로드 ---
    if (!loadPly(filePath, g_cloud)) return -1;


    // --- GLFW 초기화 및 윈도우 생성 ---
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Point Cloud Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);

    // ============================================================
    // VBO 함수 포인터 로딩
    // ============================================================
    // OpenGL 컨텍스트(glfwMakeContextCurrent)가 만들어진 후에만 가능합니다.
    // glfwGetProcAddress()는 현재 그래픽 드라이버에서 해당 이름의 함수 주소를 찾아옵니다.
    // reinterpret_cast는 "이 주소를 내가 원하는 함수 포인터 타입으로 해석해"라는 뜻입니다.
    // ============================================================
    glGenBuffers    = reinterpret_cast<PFNGLGENBUFFERSPROC>   (glfwGetProcAddress("glGenBuffers"));
    glBindBuffer    = reinterpret_cast<PFNGLBINDBUFFERPROC>   (glfwGetProcAddress("glBindBuffer"));
    glBufferData    = reinterpret_cast<PFNGLBUFFERDATAPROC>   (glfwGetProcAddress("glBufferData"));
    glDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(glfwGetProcAddress("glDeleteBuffers"));

    // 로딩 실패 시 VBO 없이 기존 방식으로 동작하도록 체크
    bool vboAvailable = glGenBuffers && glBindBuffer && glBufferData && glDeleteBuffers;
    if (!vboAvailable) {
        std::cout << "[경고] VBO를 지원하지 않는 환경입니다. 기존 방식으로 렌더링합니다." << std::endl;
    }

    // ============================================================
    // VBO 생성 및 데이터 업로드 (딱 한 번만!)
    // ============================================================
    if (vboAvailable) {
        // 1. GPU에 버퍼 "그릇" 하나를 만들고 ID를 g_vbo에 받아옵니다.
        //    유니티에서 new GameObject()를 하면 instanceID가 부여되는 것과 비슷합니다.
        glGenBuffers(1, &g_vbo);

        // 2. 방금 만든 버퍼를 "현재 작업 대상"으로 지정(바인드)합니다.
        //    이후 glBufferData 같은 명령은 이 버퍼에 적용됩니다.
        //    유니티에서 특정 GameObject를 Selection.activeObject로 선택하는 것과 비슷합니다.
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

        // 3. CPU RAM에 있던 rawData를 GPU VRAM으로 전송합니다. (이게 핵심!)
        //    GL_STATIC_DRAW: "한 번 올리고 여러 번 읽기만 할 거야" → GPU가 빠른 위치에 저장
        //    이 한 줄 이후로는 rawData를 CPU에서 직접 넘길 필요가 없어집니다.
        glBufferData(
            GL_ARRAY_BUFFER,              // 대상: 정점 데이터 버퍼
            g_cloud.rawData.size(),       // 전송할 데이터 크기 (바이트)
            g_cloud.rawData.data(),       // 전송할 데이터의 시작 주소
            GL_STATIC_DRAW                // 사용 패턴 힌트
        );

        // 4. 업로드가 끝났으니 바인딩 해제
        //    (다른 OpenGL 작업에 영향을 주지 않도록 정리하는 습관)
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        std::cout << "VBO 생성 완료! GPU VRAM에 데이터 업로드됨 (ID: " << g_vbo << ")" << std::endl;
    }
    // ============================================================

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window,     mouseButtonCallback);
    glfwSetCursorPosCallback(window,       cursorPosCallback);
    glfwSetScrollCallback(window,          scrollCallback);

    framebufferSizeCallback(window, 1280, 720);
    glPointSize(2.0f);


    // ============================================================
    // 렌더 루프
    // ============================================================
    while (!glfwWindowShouldClose(window)) {

        // 1. 화면 지우기
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2. 카메라 변환
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glScalef(g_camera.zoom,    g_camera.zoom,    g_camera.zoom);
        glRotatef(g_camera.angleX, 1.0f, 0.0f, 0.0f);
        glRotatef(g_camera.angleY, 0.0f, 1.0f, 0.0f);
        glTranslatef(-g_cloud.centerX, -g_cloud.centerY, -g_cloud.centerZ);

        // 3. 포인트 클라우드 그리기
        glEnableClientState(GL_VERTEX_ARRAY);

        if (vboAvailable) {
            // ============================================================
            // [VBO 방식] GPU VRAM의 데이터를 직접 사용
            // ============================================================
            // VBO를 바인드하면 이후 glVertexPointer의 마지막 인자가
            // "메모리 주소"가 아니라 "버퍼 안에서의 오프셋(거리)"으로 해석됩니다.
            // nullptr(=0) → 버퍼의 맨 처음부터 좌표 데이터가 있다는 뜻
            glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

            glVertexPointer(
                3,                              // x, y, z → 3개
                GL_FLOAT,                       // float 타입
                g_cloud.stride,                 // 다음 점까지의 간격
                (const void*)0                  // 오프셋 0 = 버퍼의 맨 처음부터 시작
                                                // VBO가 바인드된 상태에서는 nullptr 대신
                                                // (const void*)0 을 써야 MSVC에서 안전합니다.
            );

            if (g_cloud.colorOffset != -1) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(
                    3,                                          // R, G, B → 3개
                    GL_UNSIGNED_BYTE,                           // 1바이트 uchar
                    g_cloud.stride,                             // 다음 점까지의 간격
                    // colorOffset만큼 떨어진 위치부터 색상 시작
                    // uintptr_t: 포인터와 같은 크기의 부호없는 정수형
                    // 정수값을 포인터로 안전하게 변환할 때 사용합니다.
                    (const void*)(uintptr_t)g_cloud.colorOffset
                );
            }
        }
        else {
            // ============================================================
            // [기존 방식] CPU RAM에서 매 프레임 전송 (VBO 미지원 환경 대비)
            // ============================================================
            glVertexPointer(3, GL_FLOAT, g_cloud.stride, g_cloud.rawData.data());

            if (g_cloud.colorOffset != -1) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(3, GL_UNSIGNED_BYTE, g_cloud.stride,
                               g_cloud.rawData.data() + g_cloud.colorOffset);
            }
        }

        // 실제로 점들을 그립니다
        glDrawArrays(GL_POINTS, 0, g_cloud.vertexCount);

        // 상태 정리
        glDisableClientState(GL_VERTEX_ARRAY);
        if (g_cloud.colorOffset != -1) glDisableClientState(GL_COLOR_ARRAY);
        if (vboAvailable) glBindBuffer(GL_ARRAY_BUFFER, 0); // 바인딩 해제

        // 4. 화면 교체 & 이벤트 처리
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ============================================================
    // 정리 (유니티의 OnDestroy() 역할)
    // ============================================================
    // GPU에 만들어둔 VBO 버퍼를 명시적으로 해제합니다.
    // C++은 유니티와 달리 GC(가비지 컬렉터)가 없어서
    // 직접 만든 자원은 직접 해제해야 합니다.
    if (vboAvailable && g_vbo != 0) {
        glDeleteBuffers(1, &g_vbo);
        std::cout << "VBO 해제 완료" << std::endl;
    }

    glfwTerminate();
    return 0;
}
