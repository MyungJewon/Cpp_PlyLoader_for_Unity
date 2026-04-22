#include "PlyLoaderAPI.h"
#include "PlyParser.h"
#include "PointCloud.h"
#include <unordered_map>
#include <string>

// 핸들(정수 ID) → PointCloud 객체 매핑으로 C# ↔ C++ 간 안전한 데이터 전달을 구현합니다.
static std::unordered_map<int, PointCloud> g_clouds;
static int g_nextHandle = 0;

extern "C" {

    PLY_API int PlyLoad(const char* path)
    {
        int handle = g_nextHandle++;
        PointCloud& cloud = g_clouds[handle];

        if (!loadPly(std::string(path), cloud)) {
            g_clouds.erase(handle);
            return -1;
        }

        return handle;
    }

    PLY_API int PlyGetVertexCount(int handle)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return 0;
        return it->second.vertexCount;
    }

    PLY_API void PlyGetPositions(int handle, float* outBuffer)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;
        for (int i = 0; i < cloud.vertexCount; ++i) {
            const float* pos = reinterpret_cast<const float*>(&cloud.rawData[i * cloud.stride]);
            outBuffer[i * 3 + 0] = pos[0];
            outBuffer[i * 3 + 1] = pos[1];
            outBuffer[i * 3 + 2] = pos[2];
        }
    }

    PLY_API int PlyHasColor(int handle)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return 0;
        return (it->second.colorOffset >= 0) ? 1 : 0;
    }

    PLY_API void PlyGetColors(int handle, unsigned char* outBuffer)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;
        if (cloud.colorOffset < 0) return;

        for (int i = 0; i < cloud.vertexCount; ++i) {
            const unsigned char* color = &cloud.rawData[i * cloud.stride + cloud.colorOffset];
            outBuffer[i * 3 + 0] = color[0];
            outBuffer[i * 3 + 1] = color[1];
            outBuffer[i * 3 + 2] = color[2];
        }
    }

    PLY_API void PlyGetCenter(int handle, float* outX, float* outY, float* outZ)
    {
        auto it = g_clouds.find(handle);
        if (it == g_clouds.end()) return;

        const PointCloud& cloud = it->second;
        *outX = cloud.centerX;
        *outY = cloud.centerY;
        *outZ = cloud.centerZ;
    }

    PLY_API void PlyFree(int handle)
    {
        g_clouds.erase(handle);
    }
}
