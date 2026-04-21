#include "PlyLoaderAPI.h"
#include "PlyParser.h"
#include "PointCloud.h"
#include <unordered_map>  // handle → PointCloud 매핑에 사용
#include <string>

// ============================================================
// PlyLoaderAPI.cpp — API 함수들의 실제 구현
// ============================================================
//
// [핸들(Handle) 시스템 설계]
//
// C#과 C++ 사이에서 "어떤 포인트 클라우드를 가리키는지"를
// 안전하게 주고받기 위해 핸들(정수 ID)을 사용합니다.
//
// 구조:
//   C# → PlyLoad("파일경로") → C++ 내부에서 PointCloud 생성 → ID 반환 → C#
//   C# → PlyGetPositions(ID, buffer) → C++ 내부에서 ID로 PointCloud 찾아서 복사
//   C# → PlyFree(ID) → C++ 내부에서 PointCloud 삭제
//
// 비유: 음식점 번호표 시스템
//   손님(C#)은 번호표(handle)만 들고 있고,
//   주방(C++)은 번호에 해당하는 음식(PointCloud)을 만들고 관리합니다.
// ============================================================

// ============================================================
// 내부 상태 (이 파일 안에서만 접근 가능)
// ============================================================

// 핸들 ID → PointCloud 객체를 연결하는 해시맵
// unordered_map은 C#의 Dictionary<int, PointCloud>와 같습니다.
// 'static'은 이 변수를 이 파일(.cpp) 안에서만 접근 가능하게 제한합니다.
// (전역 변수지만 외부에서 직접 건드리지 못하게 캡슐화)
static std::unordered_map<int, PointCloud> g_clouds;

// 다음에 줄 핸들 ID. 새 PLY를 로드할 때마다 1씩 증가합니다.
// 0부터 시작하고, 실패 시 -1을 반환하므로 0도 유효한 핸들입니다.
static int g_nextHandle = 0;

// ============================================================
// API 함수 구현
// ============================================================

extern "C" {

    PLY_API int PlyLoad(const char* path)
    {
        // const char* (C 문자열)을 std::string으로 변환
        // C#의 string.ToString()처럼, 포인터로 온 문자열을 C++ 문자열로 감쌉니다.
        std::string filePath(path);

        // 새 PointCloud 객체를 핸들 맵에 추가합니다.
        // g_clouds[id] = PointCloud{} 와 같습니다.
        // 이 시점에서 PointCloud는 비어있는 상태입니다.
        int handle = g_nextHandle++;
        PointCloud& cloud = g_clouds[handle];

        // 기존 loadPly()로 파싱합니다.
        // PlyParser.cpp의 코드를 그대로 재사용하는 것이 핵심입니다.
        // 파싱 로직을 새로 짤 필요가 없습니다!
        if (!loadPly(filePath, cloud)) {
            // 실패 시: 방금 만든 빈 항목을 맵에서 제거하고 -1 반환
            g_clouds.erase(handle);
            return -1;
        }

        // 성공 시 핸들(ID) 반환
        return handle;
    }


    PLY_API int PlyGetVertexCount(int handle)
    {
        // 핸들이 유효한지 먼저 확인합니다.
        // find()는 C#의 Dictionary.ContainsKey()와 같습니다.
        // end()는 "찾지 못했음"을 나타내는 특수 반복자입니다.
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return 0; // 유효하지 않은 핸들

        return it->second.vertexCount;
        // it->second : 맵에서 값(PointCloud)을 가져옵니다.
        // it->first  : 맵에서 키(handle 숫자)를 가져옵니다.
    }


    PLY_API void PlyGetPositions(int handle, float* outBuffer)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;

        // rawData에서 각 점의 XYZ만 뽑아서 outBuffer에 순서대로 씁니다.
        //
        // rawData 구조 (점 1개 예시, stride=27):
        // [0~3]  x (float 4바이트)
        // [4~7]  y (float 4바이트)
        // [8~11] z (float 4바이트)
        // [12~23] nx, ny, nz (법선벡터, 있을 수도 없을 수도)
        // [24]   r (uchar 1바이트)
        // [25]   g
        // [26]   b
        //
        // outBuffer 구조: [x0, y0, z0, x1, y1, z1, ...]
        for (int i = 0; i < cloud.vertexCount; ++i) {
            // i번째 점의 시작 주소를 float*로 해석합니다.
            // reinterpret_cast : "이 메모리를 float 배열로 읽어라"
            const float* pos = reinterpret_cast<const float*>(
                &cloud.rawData[i * cloud.stride]
            );
            outBuffer[i * 3 + 0] = pos[0]; // X
            outBuffer[i * 3 + 1] = pos[1]; // Y
            outBuffer[i * 3 + 2] = pos[2]; // Z
        }
    }


    PLY_API int PlyHasColor(int handle)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return 0;

        // colorOffset이 -1이면 색상 없음, 0 이상이면 있음
        return (it->second.colorOffset >= 0) ? 1 : 0;
    }


    PLY_API void PlyGetColors(int handle, unsigned char* outBuffer)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;
        if (cloud.colorOffset < 0) return; // 색상 데이터 없음

        // 각 점에서 RGB 3바이트만 뽑아서 outBuffer에 씁니다.
        // outBuffer 구조: [r0, g0, b0, r1, g1, b1, ...]
        for (int i = 0; i < cloud.vertexCount; ++i) {
            // i번째 점의 시작 위치에서 colorOffset만큼 건너뛰면 R 데이터 시작
            const unsigned char* color =
                &cloud.rawData[i * cloud.stride + cloud.colorOffset];
            outBuffer[i * 3 + 0] = color[0]; // R
            outBuffer[i * 3 + 1] = color[1]; // G
            outBuffer[i * 3 + 2] = color[2]; // B
        }
    }


    PLY_API void PlyGetCenter(int handle, float* outX, float* outY, float* outZ)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;

        // 포인터로 넘겨받은 변수에 직접 씁니다.
        // C#에서 'ref float x'로 넘기면, C++에서 *outX = ... 으로 값을 돌려줄 수 있습니다.
        *outX = cloud.centerX;
        *outY = cloud.centerY;
        *outZ = cloud.centerZ;
    }


    PLY_API void PlyFree(int handle)
    {
        // 핸들에 해당하는 PointCloud를 맵에서 제거합니다.
        // erase()가 호출되면 PointCloud 소멸자가 실행되어
        // rawData(vector)의 메모리도 자동으로 해제됩니다.
        // C#의 GC처럼 자동은 아니지만, C++ vector는 소멸 시 스스로 정리합니다.
        g_clouds.erase(handle);
    }

} // extern "C" 끝
