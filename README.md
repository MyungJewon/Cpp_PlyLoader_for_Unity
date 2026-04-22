# Cpp_PlyLoader_for_Unity

C++로 작성한 PLY 포인트 클라우드 파서 및 Unity 네이티브 플러그인입니다.  
Windows / macOS 크로스플랫폼을 지원하며, 독립 실행 뷰어와 Unity 라이브러리 두 가지 형태로 빌드됩니다.

---

## 프로젝트 구조

```
Cpp_PlyLoader_for_Unity/
├── main.cpp            # 독립 실행 PLY 뷰어 (GLFW + OpenGL)
├── PlyParser.h/cpp     # PLY 파일 파서
├── PointCloud.h        # 포인트 클라우드 데이터 구조체
├── PlyLoaderAPI.h/cpp  # Unity용 extern "C" API
├── CMakeLists.txt      # 빌드 설정
└── Unity/
    ├── PlyLoaderPlugin.cs      # Unity C# 래퍼 스크립트
    └── PointCloudShader.shader # 포인트 클라우드 전용 셰이더
```

---

## 빌드 방법

### 요구사항
- CMake 3.14 이상
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2019 이상

### macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

빌드 결과물:
- `build/PointCloudViewer` — 독립 실행 뷰어
- `build/libPlyLoader.dylib` — Unity용 라이브러리

### Windows

```bash
cmake -S . -B build
cmake --build build --config Release
```

빌드 결과물:
- `build/Release/PointCloudViewer.exe`
- `build/Release/PlyLoader.dll`

---

## 독립 실행 뷰어 사용법

```bash
# 파일 경로를 인자로 전달
./build/PointCloudViewer /path/to/file.ply

# 인자 없이 실행하면 경로 입력 프롬프트가 표시됨
./build/PointCloudViewer
```

**조작 방법**

| 입력 | 동작 |
|------|------|
| 마우스 좌클릭 드래그 | 회전 |
| 마우스 스크롤 | 확대 / 축소 |

---

## Unity 플러그인 적용 방법

### 1. 라이브러리 파일 복사

| 플랫폼 | 파일 | 복사 위치 |
|--------|------|-----------|
| macOS | `libPlyLoader.dylib` | `Assets/Plugins/` |
| Windows | `PlyLoader.dll` | `Assets/Plugins/` |

### 2. Unity 스크립트 및 셰이더 복사

```
Unity/PlyLoaderPlugin.cs      → Assets/Plugins/
Unity/PointCloudShader.shader → Assets/Shaders/
```

### 3. GameObject에 컴포넌트 추가

1. 빈 GameObject 생성
2. `PlyLoaderPlugin` 컴포넌트 추가
3. Inspector에서 `Ply File Path`에 PLY 파일 절대 경로 입력
4. Play

---

## Unity API

C#에서 직접 C++ 함수를 호출할 경우:

```csharp
int handle = PlyLoad("/path/to/file.ply");   // 파일 로드, 핸들 반환
int count  = PlyGetVertexCount(handle);       // 점 개수
float[] pos = new float[count * 3];
PlyGetPositions(handle, pos);                 // XYZ 좌표 배열로 복사
PlyFree(handle);                              // 메모리 해제
```

---

## 지원 PLY 포맷

| 포맷 | 지원 여부 |
|------|-----------|
| binary_little_endian | ✅ |
| ASCII | ❌ |

---

## 플랫폼 지원

| 플랫폼 | 뷰어 | Unity 플러그인 |
|--------|------|----------------|
| macOS (Apple Silicon / Intel) | ✅ | ✅ |
| Windows | ✅ | ✅ |
| Linux | 미테스트 | 미테스트 |
