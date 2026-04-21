using System;
using System.Runtime.InteropServices; // [DllImport] 사용에 필요
using UnityEngine;

// ============================================================
// PlyLoaderPlugin.cs — C++ DLL을 Unity에서 사용하는 래퍼
// ============================================================
//
// [전체 흐름 요약]
// 1. C++ PlyLoader.dll / libPlyLoader.dylib 을 빌드합니다.
// 2. 빌드된 파일을 Unity Assets/Plugins/ 폴더에 복사합니다.
// 3. 이 스크립트를 GameObject에 붙이고 Inspector에서 PLY 경로를 지정합니다.
// 4. 게임 시작 시 자동으로 PLY를 로드해서 포인트 클라우드를 표시합니다.
//
// [배치 경로]
// Assets/
//   Plugins/
//     Windows/PlyLoader.dll          ← Windows 빌드용
//     macOS/libPlyLoader.dylib       ← macOS 빌드용
//   Scripts/
//     PlyLoaderPlugin.cs             ← 이 파일
// ============================================================

public class PlyLoaderPlugin : MonoBehaviour
{
    // ============================================================
    // [DllImport] — C++ 함수를 C#에서 호출하는 선언
    // ============================================================
    // DllImport("PlyLoader") : 플랫폼에 따라 자동으로
    //   Windows → PlyLoader.dll
    //   macOS   → libPlyLoader.dylib
    // 를 찾아서 연결합니다. (확장자/접두사는 Unity가 자동 처리)
    //
    // CallingConvention.Cdecl : C/C++ 기본 호출 규약으로 맞춥니다.
    //   호출 규약이란 "함수 인자를 어떤 순서로 스택에 넣을지"에 대한 약속입니다.
    //   C#과 C++이 같은 규약을 써야 함수가 제대로 동작합니다.
    // ============================================================
    private const string DLL_NAME = "PlyLoader";

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern int PlyLoad(string path);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern int PlyGetVertexCount(int handle);

    // float[] 배열을 넘길 때는 [Out]을 붙입니다.
    // [Out] : "C++이 이 배열에 데이터를 써서 돌려준다"는 힌트입니다.
    // 없어도 동작하지만, 명시하면 런타임이 더 효율적으로 처리합니다.
    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void PlyGetPositions(int handle, [Out] float[] outBuffer);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern int PlyHasColor(int handle);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void PlyGetColors(int handle, [Out] byte[] outBuffer);

    // C++의 float* 를 C#에서 ref float 로 받습니다.
    // ref : "이 변수의 주소를 C++에 넘겨서, C++이 직접 값을 써줄 수 있게 한다"
    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void PlyGetCenter(int handle, ref float outX, ref float outY, ref float outZ);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl)]
    private static extern void PlyFree(int handle);


    // ============================================================
    // Inspector에서 설정하는 값들
    // ============================================================
    [Header("PLY 파일 설정")]
    [Tooltip("로드할 PLY 파일의 절대 경로 또는 StreamingAssets 상대 경로")]
    public string plyFilePath = "";

    [Header("렌더링 설정")]
    [Tooltip("포인트 하나의 크기 (픽셀 단위)")]
    public float pointSize = 0.01f;

    // ============================================================
    // 내부 상태
    // ============================================================
    private int     _handle = -1;    // C++ 핸들 (-1 = 로드되지 않음)
    private Mesh    _mesh;           // Unity Mesh 객체
    private MeshFilter   _meshFilter;
    private MeshRenderer _meshRenderer;


    // ============================================================
    // Unity 생명주기: Start()
    // ============================================================
    // 게임 오브젝트가 처음 활성화될 때 한 번 호출됩니다.
    // C#의 생성자 대신 Unity에서는 Awake/Start를 씁니다.
    void Start()
    {
        if (string.IsNullOrEmpty(plyFilePath))
        {
            Debug.LogError("[PlyLoader] PLY 파일 경로가 비어있습니다.");
            return;
        }

        LoadAndDisplay(plyFilePath);
    }


    // ============================================================
    // Unity 생명주기: OnDestroy()
    // ============================================================
    // 게임 오브젝트가 파괴될 때 호출됩니다.
    // C++은 GC가 없으므로 여기서 반드시 PlyFree()를 호출해야 합니다!
    void OnDestroy()
    {
        if (_handle >= 0)
        {
            PlyFree(_handle);
            _handle = -1;
            Debug.Log("[PlyLoader] C++ 메모리 해제 완료");
        }
    }


    // ============================================================
    // PLY 로드 → Mesh 생성 → 화면 표시
    // ============================================================
    void LoadAndDisplay(string path)
    {
        // --- 1단계: C++ DLL에서 PLY 파싱 ---
        _handle = PlyLoad(path);
        if (_handle < 0)
        {
            Debug.LogError($"[PlyLoader] PLY 로드 실패: {path}");
            return;
        }

        int vertexCount = PlyGetVertexCount(_handle);
        Debug.Log($"[PlyLoader] 로드 완료 — 점 개수: {vertexCount:N0}");

        // --- 2단계: 좌표 데이터 C#으로 가져오기 ---
        // float 배열을 미리 할당합니다. (점 개수 × 3 = X,Y,Z)
        // C++ PlyGetPositions()가 이 배열에 데이터를 직접 씁니다.
        float[] positions = new float[vertexCount * 3];
        PlyGetPositions(_handle, positions);

        // float[] → Vector3[] 변환
        // Unity의 Mesh는 Vector3 배열을 씁니다.
        Vector3[] vertices = new Vector3[vertexCount];
        for (int i = 0; i < vertexCount; i++)
        {
            vertices[i] = new Vector3(
                positions[i * 3 + 0],
                positions[i * 3 + 1],
                positions[i * 3 + 2]
            );
        }

        // --- 3단계: 색상 데이터 가져오기 (있을 때만) ---
        Color32[] colors = null;
        if (PlyHasColor(_handle) == 1)
        {
            byte[] colorBytes = new byte[vertexCount * 3];
            PlyGetColors(_handle, colorBytes);

            // byte[] → Color32[] 변환
            // Color32 : R,G,B,A를 각각 0~255 byte로 저장하는 Unity 색상 구조체
            colors = new Color32[vertexCount];
            for (int i = 0; i < vertexCount; i++)
            {
                colors[i] = new Color32(
                    colorBytes[i * 3 + 0], // R
                    colorBytes[i * 3 + 1], // G
                    colorBytes[i * 3 + 2], // B
                    255                     // A (완전 불투명)
                );
            }
        }

        // --- 4단계: 무게중심 가져와서 GameObject 위치 보정 ---
        float cx = 0, cy = 0, cz = 0;
        PlyGetCenter(_handle, ref cx, ref cy, ref cz);
        // 무게중심을 원점으로 당겨서, 오브젝트가 씬 중앙에 오도록 합니다.
        // (Inspector의 Transform에서 위치를 조절하면 이 보정 기준으로 움직입니다)
        transform.position = new Vector3(-cx, -cy, -cz);
        Debug.Log($"[PlyLoader] 무게중심: ({cx:F2}, {cy:F2}, {cz:F2})");

        // --- 5단계: Unity Mesh 생성 ---
        BuildMesh(vertices, colors, vertexCount);
    }


    // ============================================================
    // Mesh 생성 및 컴포넌트 세팅
    // ============================================================
    void BuildMesh(Vector3[] vertices, Color32[] colors, int vertexCount)
    {
        _mesh = new Mesh();

        // Unity 2019.3 이상에서는 32비트 인덱스를 쓸 수 있습니다.
        // 기본(16비트)은 최대 65,535개의 점만 표현 가능합니다.
        // PLY 파일은 수백만 점이 일반적이므로 반드시 32비트로 설정해야 합니다.
        _mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;

        _mesh.vertices = vertices;

        if (colors != null)
            _mesh.colors32 = colors;

        // 포인트 클라우드는 삼각형(triangle)이 아니라 점(point) 단위로 그립니다.
        // Unity Mesh에서 점은 "인덱스 배열이 점 번호 그 자체"인 방식입니다.
        // [0, 1, 2, 3, ..., N-1] → "0번 점, 1번 점, 2번 점, ... 을 각각 찍어라"
        int[] indices = new int[vertexCount];
        for (int i = 0; i < vertexCount; i++) indices[i] = i;

        // MeshTopology.Points : 삼각형이 아닌 점으로 렌더링
        _mesh.SetIndices(indices, MeshTopology.Points, 0);

        // Bounds 수동 계산: Unity가 카메라 컬링(화면 밖 오브젝트 숨기기)에 씁니다.
        // 포인트 클라우드는 자동 계산이 느릴 수 있어서 수동으로 설정합니다.
        _mesh.RecalculateBounds();

        // --- MeshFilter, MeshRenderer 컴포넌트 추가 ---
        // Unity에서 3D 오브젝트를 화면에 그리려면 이 두 컴포넌트가 필요합니다.
        // MeshFilter   : "어떤 모양을 그릴 것인지" (Mesh 보관)
        // MeshRenderer : "어떤 재질(Material)로 그릴 것인지"
        _meshFilter = gameObject.AddComponent<MeshFilter>();
        _meshFilter.mesh = _mesh;

        _meshRenderer = gameObject.AddComponent<MeshRenderer>();

        // 포인트 렌더링에 맞는 셰이더를 사용합니다.
        // "Particles/Standard Unlit" 또는 커스텀 셰이더를 써야 점이 올바르게 보입니다.
        // 여기서는 기본 제공 셰이더를 사용합니다.
        // 더 나은 시각 품질을 원하면 커스텀 셰이더를 만들어 연결하세요.
        Material mat = new Material(Shader.Find("Particles/Standard Unlit"));
        if (mat == null)
        {
            // Particles 셰이더가 없을 경우 기본 셰이더로 대체
            mat = new Material(Shader.Find("Standard"));
            Debug.LogWarning("[PlyLoader] Particles/Standard Unlit 셰이더를 찾지 못했습니다. Standard로 대체합니다.");
        }
        _meshRenderer.material = mat;

        Debug.Log($"[PlyLoader] Mesh 생성 완료. Transform으로 위치/회전/스케일 조절 가능합니다.");
    }
}
