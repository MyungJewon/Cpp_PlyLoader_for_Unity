cmake_minimum_required(VERSION 3.14)
project(PointCloudViewer)

# ============================================================
# 1. GLFW 다운로드 및 준비
# ============================================================
# FetchContent: 외부 라이브러리를 CMake가 자동으로 다운로드/빌드합니다.
# npm install, pip install 같은 역할이라고 생각하면 됩니다.
# GLFW는 창 생성과 입력 처리를 담당합니다. (뷰어 실행파일에서만 사용)
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

# ============================================================
# 2. OpenGL 찾기
# ============================================================
find_package(OpenGL REQUIRED)

# ============================================================
# 3. 뷰어 실행파일 (기존 standalone 뷰어, 개발/테스트용)
# ============================================================
# add_executable : 실행 가능한 프로그램(.exe / 파일)을 만듭니다.
add_executable(PointCloudViewer
    main.cpp
    PlyParser.cpp
)
target_link_libraries(PointCloudViewer PRIVATE glfw OpenGL::GL)

# ============================================================
# 4. Unity용 공유 라이브러리 (핵심 추가 부분!)
# ============================================================
# add_library(이름 SHARED 소스파일...)
#   SHARED : 공유 라이브러리를 만듭니다.
#            Windows → PlyLoader.dll
#            macOS   → libPlyLoader.dylib
#            Linux   → libPlyLoader.so
#
# main.cpp과 GLFW/OpenGL은 포함하지 않습니다.
# Unity가 렌더링을 담당하므로, 파싱 코드만 라이브러리에 넣습니다.
add_library(PlyLoader SHARED
    PlyLoaderAPI.cpp   # Unity C# 에서 호출할 extern "C" API
    PlyParser.cpp      # 실제 PLY 파싱 로직 (재사용!)
)

# macOS에서 심볼(함수 이름)을 기본적으로 숨기고
# PLY_API(__attribute__((visibility("default"))))가 붙은 것만 공개합니다.
# 이렇게 하면 불필요한 내부 함수가 외부에 노출되지 않아 깔끔해집니다.
if(NOT MSVC)
    target_compile_options(PlyLoader PRIVATE -fvisibility=hidden)
endif()

# MSVC(Windows)에서 한글 주석 깨짐 방지
if(MSVC)
    target_compile_options(PointCloudViewer PRIVATE /utf-8)
    target_compile_options(PlyLoader PRIVATE /utf-8)
endif()

# ============================================================
# [Unity에 넣는 방법 - 빌드 후 할 일]
# ============================================================
# 빌드하면 build/ 폴더 안에 라이브러리 파일이 생깁니다:
#   Windows : build/PlyLoader.dll
#   macOS   : build/libPlyLoader.dylib
#
# 이 파일을 Unity 프로젝트의 아래 경로에 복사합니다:
#   Assets/Plugins/Windows/PlyLoader.dll    (윈도우용)
#   Assets/Plugins/macOS/libPlyLoader.dylib (맥용)
#
# Unity가 플랫폼에 따라 자동으로 맞는 파일을 사용합니다.
# ============================================================
