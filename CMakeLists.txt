cmake_minimum_required(VERSION 3.14)
project(PointCloudViewer)

# 1. 패키지 매니저 활성화 및 GLFW 다운로드 설정
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. 실행 파일 생성 (전체 파일에서 딱 한 번만 있어야 합니다!)
add_executable(PointCloudViewer
    main.cpp
    PlyParser.cpp
)

# 3. 내장 OpenGL 찾기 및 라이브러리 연결
find_package(OpenGL REQUIRED)
target_link_libraries(PointCloudViewer PRIVATE glfw OpenGL::GL)