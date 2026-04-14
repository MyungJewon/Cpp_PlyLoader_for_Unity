#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include "PointCloud.h"
#include "PlyParser.h"

// ============================================================
// main.cpp — 진입점 + 카메라 + 렌더 루프
// ============================================================
// 이 파일이 하는 일:
//   1. 파일 경로 받기
//   2. PlyParser에게 파일 읽기 위임
//   3. 카메라 상태 관리 (마우스/스크롤 입력)
//   4. OpenGL로 화면에 그리기 (렌더 루프)
//
// 파일을 어떻게 파싱하는지는 신경 쓰지 않습니다.
// → PlyParser가 알아서 처리하고 PointCloud에 담아줍니다.
// ============================================================


// ============================================================
// 카메라 상태 구조체
// ============================================================
// 마우스 드래그, 줌 등 카메라 관련 상태를 한 곳에 모아뒀습니다.
// 유니티의 Camera 컴포넌트에 있는 변수들과 비슷한 개념입니다.
//
// [왜 구조체로 묶나요?]
// 이전에는 전역 변수가 코드 여기저기 흩어져 있었습니다.
// 구조체로 묶으면 "카메라 관련 데이터"가 어디 있는지 한눈에 보이고,
// 나중에 카메라를 여러 개 만들거나 함수에 넘길 때도 편합니다.
// ============================================================
struct Camera {
    float zoom    = 1.0f;  // 확대/축소 배율 (스크롤로 조절)
    float angleX  = 0.0f;  // X축 회전 각도 (마우스 상하 드래그)
    float angleY  = 0.0f;  // Y축 회전 각도 (마우스 좌우 드래그)

    bool   isDragging = false; // 마우스 버튼이 눌려있는 상태인지
    double lastMouseX = 0.0;   // 이전 프레임의 마우스 X 위치 (드래그 속도 계산용)
    double lastMouseY = 0.0;   // 이전 프레임의 마우스 Y 위치
};


// ============================================================
// 전역 인스턴스
// ============================================================
// GLFW 콜백 함수들은 C 스타일 함수라서 클래스/구조체 멤버가 될 수 없습니다.
// 그래서 콜백에서 접근할 수 있도록 전역으로 선언합니다.
//
// [나중에 개선할 수 있는 방법]
// glfwSetWindowUserPointer(window, &g_camera) 를 쓰면
// 전역 변수 없이도 콜백에서 카메라에 접근할 수 있습니다.
// 이건 3단계에서 도전해볼 과제입니다!
// ============================================================
Camera     g_camera;
PointCloud g_cloud;


// ============================================================
// GLFW 콜백 함수들
// ============================================================
// 콜백(Callback)이란? "특정 이벤트가 발생하면 GLFW가 자동으로 호출해주는 함수"
// 유니티의 Update(), OnMouseDown() 같은 이벤트 함수와 비슷합니다.
// ============================================================

// 마우스 버튼 클릭/릴리즈 이벤트
void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // 왼쪽 버튼을 눌렀다 → 드래그 시작
            g_camera.isDragging = true;
            // 현재 마우스 위치를 기준점으로 기억해둡니다.
            // (다음 이동량을 계산할 때 이 위치와 비교합니다)
            glfwGetCursorPos(glfwGetCurrentContext(), &g_camera.lastMouseX, &g_camera.lastMouseY);
        }
        else if (action == GLFW_RELEASE) {
            // 왼쪽 버튼을 뗐다 → 드래그 종료
            g_camera.isDragging = false;
        }
    }
}

// 마우스 커서 이동 이벤트
void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
    if (g_camera.isDragging) {
        // 이전 위치와 현재 위치의 차이(delta)로 회전량을 계산합니다.
        // 0.5f는 감도(sensitivity) — 값이 클수록 빠르게 회전합니다.
        g_camera.angleY += (float)(xpos - g_camera.lastMouseX) * 0.5f; // 좌우 드래그 → Y축 회전
        g_camera.angleX += (float)(ypos - g_camera.lastMouseY) * 0.5f; // 상하 드래그 → X축 회전

        // 현재 위치를 다음 프레임의 "이전 위치"로 업데이트
        g_camera.lastMouseX = xpos;
        g_camera.lastMouseY = ypos;
    }
}

// 마우스 스크롤 이벤트
void scrollCallback(GLFWwindow*, double, double yoffset) {
    // yoffset: 위로 굴리면 +1, 아래로 굴리면 -1
    g_camera.zoom += (float)yoffset * 0.1f;

    // 줌이 너무 작아지면 화면이 뒤집어지므로 최소값 제한
    if (g_camera.zoom < 0.1f) g_camera.zoom = 0.1f;
}

// 창 크기 변경 이벤트 (창을 늘리거나 줄일 때 자동 호출)
void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    // 뷰포트: OpenGL이 그림을 그릴 화면 영역을 창 크기에 맞게 업데이트
    glViewport(0, 0, width, height);

    // 투영 행렬(카메라 렌즈) 재설정
    // 창 비율이 바뀌면 그림이 찌그러지지 않도록 aspect ratio를 반영합니다.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)width / (float)height;
    float s = g_cloud.orthoSize;
    if (width >= height)
        glOrtho(-s * aspect, s * aspect, -s, s, -s * 2.0f, s * 2.0f); // 가로가 긴 창
    else
        glOrtho(-s, s, -s / aspect, s / aspect, -s * 2.0f, s * 2.0f); // 세로가 긴 창
}


// ============================================================
// main() — 프로그램 시작점
// ============================================================
// int argc  : 실행 시 전달된 인자의 개수
// char* argv[]: 실행 인자 배열
//   argv[0] = 실행 파일 경로 (예: "C:/.../ PointCloudViewer.exe")
//   argv[1] = 첫 번째 인자 (예: 드래그 앤 드롭한 파일 경로)
// ============================================================
int main(int argc, char* argv[]) {

    // --- 파일 경로 결정 ---
    std::string filePath;

    if (argc >= 2) {
        // 실행 인자가 있으면 첫 번째 인자를 파일 경로로 사용합니다.
        // Windows 탐색기에서 .exe 위에 파일을 드래그 앤 드롭하면
        // 그 파일 경로가 argv[1]로 자동으로 들어옵니다!
        filePath = argv[1];
    }
    else {
        // 인자가 없으면 콘솔에서 직접 입력받습니다.
        std::cout << "PLY 파일 경로를 입력하세요: ";
        std::getline(std::cin, filePath);

        // 탐색기에서 경로를 복사해서 붙여넣으면 앞뒤에 따옴표("")가 붙는 경우가 있습니다.
        // 예: "C:/Users/Deepfine/Desktop/fused.ply"  → 따옴표 제거
        if (!filePath.empty() && filePath.front() == '"') filePath = filePath.substr(1);
        if (!filePath.empty() && filePath.back()  == '"') filePath.pop_back();
    }

    // --- PLY 파일 로드 ---
    // loadPly()는 PlyParser.cpp에 구현되어 있습니다.
    // 성공하면 g_cloud에 데이터가 채워지고 true가 반환됩니다.
    // 실패하면 false가 반환되어 프로그램을 종료합니다.
    if (!loadPly(filePath, g_cloud)) return -1;


    // --- GLFW 초기화 및 윈도우 생성 ---
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Point Cloud Viewer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    // 이 창을 OpenGL의 현재 작업 대상으로 지정합니다.
    glfwMakeContextCurrent(window);

    // 이벤트 발생 시 호출될 콜백 함수들을 등록합니다.
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window,     mouseButtonCallback);
    glfwSetCursorPosCallback(window,       cursorPosCallback);
    glfwSetScrollCallback(window,          scrollCallback);

    // 초기 창 크기로 투영 행렬을 한 번 설정합니다.
    framebufferSizeCallback(window, 1280, 720);

    // 점의 크기를 2픽셀로 설정 (1이면 너무 작아서 잘 안 보임)
    glPointSize(2.0f);


    // ============================================================
    // 렌더 루프 (유니티의 Update() + OnRenderObject() 역할)
    // ============================================================
    // 창이 닫힐 때까지 매 프레임 반복합니다.
    // 1프레임 = 화면 지우기 → 카메라 변환 → 점 그리기 → 화면 교체
    // ============================================================
    while (!glfwWindowShouldClose(window)) {

        // 1. 이전 프레임 화면을 지웁니다 (어두운 파란색으로)
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2. 카메라 변환 행렬을 설정합니다.
        //    순서가 중요합니다: Scale → Rotate → Translate 순으로 적용됩니다.
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glScalef(g_camera.zoom,   g_camera.zoom,   g_camera.zoom);   // 줌
        glRotatef(g_camera.angleX, 1.0f, 0.0f, 0.0f);                // X축 회전
        glRotatef(g_camera.angleY, 0.0f, 1.0f, 0.0f);                // Y축 회전
        glTranslatef(-g_cloud.centerX, -g_cloud.centerY, -g_cloud.centerZ); // 무게중심을 원점으로

        // 3. 포인트 클라우드를 그립니다.
        //    "배열 형태의 데이터를 GPU에 넘겨서 한 번에 그린다"는 방식입니다.

        // 좌표 데이터 배열 연결
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(
            3,                         // 점 1개당 값 개수 (x, y, z → 3개)
            GL_FLOAT,                  // 각 값의 타입 (4바이트 float)
            g_cloud.stride,            // 다음 점까지의 간격 (바이트)
            g_cloud.rawData.data()     // 데이터 배열의 시작 주소
        );

        // 색상 데이터 배열 연결 (색상이 있는 파일만)
        if (g_cloud.colorOffset != -1) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(
                3,                                              // R, G, B → 3개
                GL_UNSIGNED_BYTE,                               // 각 값의 타입 (1바이트 uchar)
                g_cloud.stride,                                 // 다음 점까지의 간격
                g_cloud.rawData.data() + g_cloud.colorOffset   // 색상 데이터 시작 주소
            );
        }

        // 연결된 배열로 점들을 한 번에 그립니다.
        glDrawArrays(GL_POINTS, 0, g_cloud.vertexCount);

        // 사용한 상태 정리 (다음 프레임에서 오작동 방지)
        glDisableClientState(GL_VERTEX_ARRAY);
        if (g_cloud.colorOffset != -1) glDisableClientState(GL_COLOR_ARRAY);

        // 4. 백버퍼(그린 내용)를 화면에 표시합니다.
        //    더블 버퍼링: 보이는 화면(프론트)과 그리는 화면(백)을 교체합니다.
        //    → 화면이 깜빡이지 않는 이유입니다.
        glfwSwapBuffers(window);

        // 5. 마우스, 키보드 등 이벤트를 처리합니다.
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
