#pragma once

// Unity용 extern "C" API
// Windows: PlyLoader.dll / macOS: libPlyLoader.dylib

#ifdef _WIN32
    #define PLY_API __declspec(dllexport)
#else
    #define PLY_API __attribute__((visibility("default")))
#endif

extern "C" {
    PLY_API int  PlyLoad(const char* path);
    PLY_API int  PlyGetVertexCount(int handle);
    PLY_API void PlyGetPositions(int handle, float* outBuffer);
    PLY_API int  PlyHasColor(int handle);
    PLY_API void PlyGetColors(int handle, unsigned char* outBuffer);
    PLY_API void PlyGetCenter(int handle, float* outX, float* outY, float* outZ);
    PLY_API void PlyFree(int handle);
}
