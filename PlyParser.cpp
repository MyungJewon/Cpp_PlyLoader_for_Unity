#include "PlyParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// PLY 파일을 파싱해 PointCloud 구조체에 데이터를 채웁니다.
bool loadPly(const std::string &filePath, PointCloud &cloud)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "파일을 열 수 없습니다: " << filePath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.find("element vertex") != std::string::npos)
        {
            std::stringstream ss(line);
            std::string dummy;
            ss >> dummy >> dummy >> cloud.vertexCount;
        }
        else if (line.find("property float") != std::string::npos)
        {
            cloud.stride += 4;
        }
        else if (line.find("property uchar") != std::string::npos)
        {
            if (line.find("red") != std::string::npos)
                cloud.colorOffset = cloud.stride;
            cloud.stride += 1;
        }
        else if (line == "end_header")
        {
            break;
        }
    }

    std::cout << "Vertex Count : " << cloud.vertexCount << std::endl;
    std::cout << "Stride       : " << cloud.stride << " bytes" << std::endl;
    std::cout << "Color Offset : " << (cloud.colorOffset == -1 ? "없음" : std::to_string(cloud.colorOffset)) << std::endl;

    cloud.rawData.resize(cloud.vertexCount * cloud.stride);
    file.read(reinterpret_cast<char *>(cloud.rawData.data()), cloud.rawData.size());
    file.close();

    if (cloud.vertexCount > 0)
    {
        float sumX = 0, sumY = 0, sumZ = 0;
        for (int i = 0; i < cloud.vertexCount; ++i)
        {
            float *pos = reinterpret_cast<float *>(&cloud.rawData[i * cloud.stride]);
            sumX += pos[0];
            sumY += pos[1];
            sumZ += pos[2];
        }
        cloud.centerX = sumX / cloud.vertexCount;
        cloud.centerY = sumY / cloud.vertexCount;
        cloud.centerZ = sumZ / cloud.vertexCount;

        float maxDist = 0.0f;
        for (int i = 0; i < cloud.vertexCount; ++i)
        {
            float *pos = reinterpret_cast<float *>(&cloud.rawData[i * cloud.stride]);
            float dist = std::max({std::abs(pos[0] - cloud.centerX),
                                   std::abs(pos[1] - cloud.centerY),
                                   std::abs(pos[2] - cloud.centerZ)});
            if (dist > maxDist) maxDist = dist;
        }
        cloud.orthoSize = maxDist * 1.2f;
    }

    return true;
}
