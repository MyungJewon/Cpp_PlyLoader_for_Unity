#include "PlyParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// ============================================================
// PlyParser.cpp — PLY 파일 파서의 "실제 구현부"
// ============================================================
// PlyParser.h에서 선언한 loadPly() 함수의 실제 내용이 여기 있습니다.
// main.cpp는 "어떻게 파싱하는지" 전혀 몰라도 됩니다.
// 그냥 loadPly()를 호출하면 결과만 받아가면 됩니다.
// → 이것이 "관심사의 분리(Separation of Concerns)"입니다.
// ============================================================

bool loadPly(const std::string &filePath, PointCloud &cloud)
{

    // === 파일 열기 ===
    // std::ios::binary → 텍스트 모드가 아닌 바이너리 모드로 열기
    // PLY 파일은 헤더는 텍스트, 나머지는 바이너리라서 반드시 binary 모드로 열어야 합니다.
    // 텍스트 모드로 열면 Windows에서 줄바꿈(\r\n)을 자동 변환해버려서 데이터가 깨집니다!
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "파일을 열 수 없습니다: " << filePath << std::endl;
        return false; // 실패 → main에서 종료 처리
    }

    // ============================================================
    // === 1단계: 헤더 분석 (파일 구조 파악) ===
    // ============================================================
    // PLY 파일의 맨 위쪽은 이런 텍스트로 시작합니다:
    //
    //   ply
    //   format binary_little_endian 1.0
    //   element vertex 1200000       ← 점이 몇 개인지
    //   property float x             ← x 좌표 (4바이트 float)
    //   property float y
    //   property float z
    //   property float nx            ← 법선벡터 (없는 파일도 있음)
    //   property float ny
    //   property float nz
    //   property uchar red           ← 색상 (1바이트 uchar)
    //   property uchar green
    //   property uchar blue
    //   end_header                   ← 여기서부터 바이너리 데이터 시작
    //
    // 이 헤더를 한 줄씩 읽으면서 파일 구조를 파악합니다.

    std::string line;
    while (std::getline(file, line))
    {
        // Windows에서 만든 PLY 파일은 줄바꿈이 \r\n입니다.
        // 바이너리 모드로 열면 \r이 자동 제거되지 않아서
        // "end_header\r" 처럼 끝에 \r이 붙어 비교가 실패합니다.
        // 그래서 매 줄마다 끝의 \r을 명시적으로 제거합니다.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.find("element vertex") != std::string::npos)
        {
            // "element vertex 1200000" 같은 줄에서 숫자만 뽑아냅니다.
            // std::stringstream은 문자열을 >> 연산자로 단어 단위로 쪼갤 수 있습니다.
            std::stringstream ss(line);
            std::string dummy; // "element", "vertex" 두 단어는 버립니다
            ss >> dummy >> dummy >> cloud.vertexCount;
        }
        else if (line.find("property float") != std::string::npos)
        {
            // float 타입 속성을 발견할 때마다 stride를 4바이트씩 늘립니다.
            // x, y, z, nx, ny, nz 각각 4바이트이므로 6개면 총 24바이트
            cloud.stride += 4;
        }
        else if (line.find("property uchar") != std::string::npos)
        {
            if (line.find("red") != std::string::npos)
            {
                // "red"를 발견한 순간의 stride 위치 = 색상 데이터가 시작되는 바이트 번호
                // 예) stride가 24일 때 red 발견 → colorOffset = 24
                // 즉, 점 데이터의 24번째 바이트부터가 RGB
                cloud.colorOffset = cloud.stride;
            }
            // uchar 타입은 1바이트
            cloud.stride += 1;
        }
        else if (line == "end_header")
        {
            // 헤더 끝! 다음 줄부터는 바이너리 데이터이므로 루프를 빠져나갑니다.
            break;
        }
    }

    std::cout << "Vertex Count : " << cloud.vertexCount << "개" << std::endl;
    std::cout << "Stride       : " << cloud.stride << " bytes/점" << std::endl;
    std::cout << "Color Offset : " << (cloud.colorOffset == -1 ? "없음" : std::to_string(cloud.colorOffset) + "번째 바이트") << std::endl;

    // ============================================================
    // === 2단계: 바이너리 데이터 통째로 읽기 ===
    // ============================================================
    // 헤더 분석으로 "점 1개 크기(stride)"와 "점 개수(vertexCount)"를 알았으니
    // 필요한 총 메모리 크기 = vertexCount * stride 바이트
    //
    // resize()로 그 크기만큼 벡터 공간을 확보하고,
    // file.read()로 남은 파일 내용을 통째로 그 공간에 쏟아붓습니다.
    // 점 하나하나를 루프로 읽는 것보다 훨씬 빠릅니다!

    cloud.rawData.resize(cloud.vertexCount * cloud.stride);

    // reinterpret_cast<char*> : rawData의 포인터를 char* 타입으로 변환
    // file.read()는 char* 타입만 받기 때문에 이 변환이 필요합니다.
    // 데이터 내용은 전혀 바뀌지 않고, 타입 딱지만 바꾸는 것입니다.
    file.read(reinterpret_cast<char *>(cloud.rawData.data()), cloud.rawData.size());
    file.close();

    // ============================================================
    // === 3단계: 무게중심 및 카메라 크기 자동 계산 ===
    // ============================================================
    // 어떤 PLY 파일이든 카메라가 자동으로 딱 맞게 보이도록
    // 전체 점들의 평균 위치(무게중심)와 가장 먼 점까지의 거리를 계산합니다.

    if (cloud.vertexCount > 0)
    {

        // --- 무게중심 계산 ---
        float sumX = 0, sumY = 0, sumZ = 0;
        for (int i = 0; i < cloud.vertexCount; ++i)
        {
            // rawData에서 i번째 점의 시작 주소를 float* 로 해석합니다.
            // → 그 위치부터 4바이트씩 읽으면 pos[0]=x, pos[1]=y, pos[2]=z
            float *pos = reinterpret_cast<float *>(&cloud.rawData[i * cloud.stride]);
            sumX += pos[0];
            sumY += pos[1];
            sumZ += pos[2];
        }
        cloud.centerX = sumX / cloud.vertexCount;
        cloud.centerY = sumY / cloud.vertexCount;
        cloud.centerZ = sumZ / cloud.vertexCount;

        // --- 카메라 크기(orthoSize) 계산 ---
        // 무게중심으로부터 가장 멀리 떨어진 점을 찾아서
        // 그 거리의 1.2배를 카메라 크기로 설정합니다. (약간 여유 있게)
        float maxDist = 0.0f;
        for (int i = 0; i < cloud.vertexCount; ++i)
        {
            float *pos = reinterpret_cast<float *>(&cloud.rawData[i * cloud.stride]);
            // X, Y, Z 방향 중 가장 큰 편차를 이 점의 "거리"로 봅니다.
            float dist = std::max({std::abs(pos[0] - cloud.centerX),
                                   std::abs(pos[1] - cloud.centerY),
                                   std::abs(pos[2] - cloud.centerZ)});
            if (dist > maxDist)
                maxDist = dist;
        }
        cloud.orthoSize = maxDist * 1.2f;

        std::cout << "무게중심 : (" << cloud.centerX << ", " << cloud.centerY << ", " << cloud.centerZ << ")" << std::endl;
        std::cout << "카메라 크기 : " << cloud.orthoSize << std::endl;
    }

    return true; // 성공!
}
