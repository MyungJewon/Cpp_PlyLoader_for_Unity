// ============================================================
// PointCloudShader.shader — 포인트 클라우드 전용 셰이더
// ============================================================
// Unity에서 MeshTopology.Points로 그리려면
// 꼭 PSIZE 출력을 지원하는 셰이더가 필요합니다.
// 일반 셰이더(Standard, Unlit 등)는 PSIZE를 모르기 때문에
// 점 크기 조절이 안 되고 에러가 납니다.
//
// [배치 경로]
// Assets/Shaders/PointCloudShader.shader
// ============================================================

Shader "Custom/PointCloud"
{
    Properties
    {
        // Inspector에서 조절할 수 있는 점 크기
        // 숫자가 클수록 점이 크게 보입니다.
        _PointSize ("Point Size", Float) = 5.0
    }

    SubShader
    {
        // RenderType: 불투명 오브젝트로 분류
        // Queue: 일반 불투명 렌더링 순서
        Tags { "RenderType"="Opaque" "Queue"="Geometry" }

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            // --------------------------------------------------------
            // 셰이더 입력 구조체 (CPU → 버텍스 셰이더)
            // --------------------------------------------------------
            struct appdata
            {
                float4 vertex : POSITION;   // 점의 3D 위치 (XYZ)
                float4 color  : COLOR;      // 점의 색상 (RGBA)
            };

            // --------------------------------------------------------
            // 셰이더 출력 구조체 (버텍스 셰이더 → 프래그먼트 셰이더)
            // --------------------------------------------------------
            struct v2f
            {
                float4 pos   : SV_POSITION; // 화면 좌표로 변환된 위치
                float4 color : COLOR;       // 그대로 전달할 색상
                float  size  : PSIZE;       // 점의 픽셀 크기 (이게 핵심!)
                                            // PSIZE: GPU에게 "이 점을 몇 픽셀로 그려라"
                                            // 이 출력이 없으면 Unity가 경고를 냅니다.
            };

            float _PointSize; // Properties에서 선언한 변수를 여기서 받아옵니다.

            // --------------------------------------------------------
            // 버텍스 셰이더: 점 하나하나의 위치와 크기를 계산합니다.
            // CPU의 Mesh.vertices 배열에서 점마다 한 번씩 실행됩니다.
            // --------------------------------------------------------
            v2f vert(appdata v)
            {
                v2f o;
                // UnityObjectToClipPos: 3D 월드 좌표 → 화면 2D 좌표 변환
                // Unity가 카메라, 투영 행렬을 자동으로 적용해줍니다.
                o.pos   = UnityObjectToClipPos(v.vertex);
                o.color = v.color;
                o.size  = _PointSize;
                return o;
            }

            // --------------------------------------------------------
            // 프래그먼트 셰이더: 각 점의 최종 색상을 결정합니다.
            // 버텍스 셰이더가 정한 크기만큼의 픽셀 영역에 실행됩니다.
            // --------------------------------------------------------
            fixed4 frag(v2f i) : SV_Target
            {
                // 버텍스에서 받아온 색상을 그대로 출력합니다.
                // 조명 계산 없음 (Unlit) → 빠르고 PLY 원본 색상 그대로
                return i.color;
            }

            ENDCG
        }
    }

    // Custom 셰이더를 지원하지 않는 환경에서 대체할 셰이더
    Fallback "Unlit/Color"
}
