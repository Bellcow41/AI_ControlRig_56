# AI SetUp Tool (UE 5.6) 🎮

> UE5.6 플러그인 - AI 기반 본 매핑으로 Control Rig, IK Rig, Kawaii Physics AnimBP, Physics Asset 자동 생성

## 버전 정보

| 항목 | 값 |
|------|-----|
| 엔진 버전 | Unreal Engine 5.6 |
| 플러그인 이름 | `AI_SetUpTool_56_V1` |
| 프로젝트 경로 | `E:\AI\AI_SetUpTool_56` |

> 📌 5.7 버전은 `E:\AI\AI_ControlRig_02` 참조

## 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| 🧠 **AI Bone Mapping** | 체인 기반 분석 + 키워드 매칭 | ✅ 완료 |
| ✨ **Create Control Rig** | 템플릿 복제 + 자동 본 리매핑 | ✅ 완료 |
| 🦴 **IK Rig Generator** | IK Rig + Retargeter 자동 생성 | ✅ 완료 |
| 🎀 **Kawaii Physics** | 태그 기반 세컨더리 본 AnimBP 자동 생성 | ✅ 완료 |
| ⚡ **Physics Asset** | 메인 본 캡슐 콜리전 자동 생성 | ✅ 완료 |
| 🎯 **Anim Picker** | Control Rig 컨트롤러 시각적 선택 도구 | ✅ 개발중 (대부분 완료) |

## 플러그인 탭 구성

| 탭 | 기능 | 색상 테마 | 상태 |
|----|------|-----------|------|
| **Control Rig** | AI 본 매핑 + Control Rig 자동 생성 | 파랑 | ✅ 완료 |
| **IK Rig** | IK Rig + Retargeter 자동 생성 | 파랑 | ✅ 완료 |
| **Kawaii Physics** | Secondary 본용 AnimBP 자동 생성 | 핑크 | ✅ 완료 |
| **Physics Asset** | 메인 본 캡슐 콜리전 자동 생성 | 주황 | ✅ 완료 |
| **Anim Picker** | Control Rig 컨트롤러 시각적 선택 도구 | 청록 | ✅ 개발중 |

## Anim Picker 탭 기능

### ✅ 구현 완료된 기능

#### 뷰 모드
- **List View**: Space별 그룹화, 접기/펼치기, 다중 선택
- **2D View**: 사람 형태 고정 레이아웃, 메인/세컨더리 영역 분리
- **3D View**: 3D 뷰포트에 컨트롤러 마커 표시

#### 시퀀서 연동
- ✅ **자동 연동**: 시퀀서에서 컨트롤러 선택 시 자동으로 해당 Control Rig 로드
- ✅ **다중 캐릭터 지원**: 여러 캐릭터 Control Rig 간 자동 전환
- ✅ **선택 동기화**: 피커 클릭 시 시퀀서에서 실제 컨트롤러 선택

#### 2D View 기능
- ✅ **영역 구분**: MAIN CONTROLLERS / SECONDARY 라벨 및 구분선
- ✅ **세컨더리 정리**: Space별 라벨 (예: "Spine Space") + 체인별 정렬
- ✅ **뷰포트 조작**:
  - 마우스 휠 버튼 드래그: 패닝
  - Ctrl + 드래그: 마우스 커서 중심 줌
  - 마우스 휠: 줌 인/아웃
- ✅ **피커 조작**:
  - 드래그: 피커 위치 이동
  - 코너 핸들 드래그: 피커 크기 조절
- ✅ **다중 선택**:
  - 일반 클릭: 단일 선택
  - Shift + 클릭: 추가 선택
  - Ctrl + 클릭: 토글 선택
  - 빈 공간 클릭: 전체 해제

#### Maya 스타일 기능
- ✅ **Reset/Zero**: 선택된 컨트롤러 초기화
- ✅ **Key Selected**: 선택된 컨트롤러에 키프레임
- ✅ **Mirror Pose**: 좌우 대칭 복사
- ✅ **Pose Library**: 포즈 저장/불러오기

#### 커스텀 피커 그룹
- ✅ **Create New Picker**: 선택한 컨트롤러들로 새 피커 버튼 생성
- ✅ **시퀀서 선택 지원**: 시퀀서에서 선택한 컨트롤러로도 피커 생성 가능
- ✅ **그룹 관리**: 이름 변경, 삭제, 목록 관리

#### 레이아웃 에셋 시스템
- ✅ **AnimPickerLayoutAsset**: 커스텀 레이아웃을 UAsset으로 저장
- ✅ **Control Rig별 관리**: 각 Control Rig마다 여러 레이아웃 저장 가능
- ✅ **팀 공유**: 레이아웃 에셋을 팀원들과 공유 가능

#### 기타
- ✅ **툴팁**: 피커 호버 시 컨트롤러 이름 표시
- ✅ **Shape 정보 추출**: ShapeName, ShapeTransform, ShapeColor
- ✅ **필터링**: `_ctrl` 및 `_switch` 컨트롤러 지원

### 🔧 남은 작업
- 레이아웃 에셋 이름 변경 기능
- 포즈 라이브러리를 UControlRigPoseAsset으로 영구 저장

## 프로젝트 구조

```
AI_SetUpTool_56/
├── Plugins/
│   └── AI_SetUpTool_56_V1/          # UE5.6 플러그인
│       ├── Source/AI_SetUpTool_56_V1/
│       │   ├── Public/
│       │   │   ├── SControlRigToolWidget.h
│       │   │   ├── SAnimPickerViewport.h     # 3D 뷰포트
│       │   │   └── AnimPickerLayoutAsset.h   # 레이아웃 에셋
│       │   └── Private/
│       │       ├── SControlRigToolWidget.cpp
│       │       ├── SAnimPickerViewport.cpp
│       │       ├── AnimPickerLayoutAsset.cpp
│       │       └── ControlRigToolModule.cpp
│       ├── BoneMapping_AI/          # AI 시스템
│       │   ├── python/              # 임베디드 Python 3.11
│       │   ├── 03_fine_tuning/      # LoRA 모델
│       │   └── 04_inference/        # API 서버
│       ├── Resources/               # 템플릿 Control Rig
│       └── install.bat              # Python 환경 자동 설치
├── Content/
│   └── 00_CooT/
│       ├── test_rig_fk.uasset       # FK 모드 테스트 Control Rig
│       └── test_rig_ik.uasset       # IK 모드 테스트 Control Rig
└── README.md
```

## 설치 및 실행

### 1. 플러그인 설치
1. `Plugins/AI_SetUpTool_56_V1` 폴더를 프로젝트의 `Plugins` 폴더에 복사
2. `Binaries`, `Intermediate` 폴더가 있으면 삭제 (첫 실행 시 자동 빌드)
3. `install.bat` 실행 (Python 패키지 자동 설치)
4. 언리얼 에디터 실행

### 2. 에디터에서 사용
1. **Tools → AI Control Rig Tool**
2. 원하는 탭 선택 (Control Rig / IK Rig / Kawaii Physics / Physics Asset / Anim Picker)
3. 에셋 선택 후 생성 버튼 클릭

### 3. Anim Picker 사용법

#### 2D View 조작
| 조작 | 동작 |
|------|------|
| 좌클릭 | 피커 선택 |
| Shift + 좌클릭 | 추가 선택 |
| Ctrl + 좌클릭 | 토글 선택 |
| 휠 버튼 드래그 | 뷰포트 패닝 |
| Ctrl + 드래그 | 줌 (마우스 커서 중심) |
| 피커 드래그 | 피커 위치 이동 |
| 코너 핸들 드래그 | 피커 크기 조절 |

### 4. 수동 빌드 (개발용)
```bash
cd "D:\01_Works\00_UE\UE_5.6\Engine\Build\BatchFiles"
.\Build.bat AI_SetUpTool_56Editor Win64 Development -Project="E:\AI\AI_SetUpTool_56\AI_SetUpTool_56.uproject"
```

## 5.7과의 차이점

| 항목 | 5.6 | 5.7 |
|------|-----|-----|
| 플러그인 이름 | `AI_SetUpTool_56_V1` | `AI_SetUpTool_57` |
| ControlRigBlueprint | `ControlRigBlueprint.h` | `ControlRigBlueprintLegacy.h` |
| CalcBoneVertInfos | 3개 인자 | 4개 인자 |
| FSavePackageArgs | `#include "UObject/SavePackage.h"` 필요 | 기본 포함 |
| Anim Picker | ✅ 개발중 (대부분 완료) | ❌ 미구현 |

## 배포 (다른 컴퓨터에 설치)

### 포함된 파일
```
Plugins/AI_SetUpTool_56_V1/
├── BoneMapping_AI/
│   └── python/              # Python 3.11 임베디드
├── install.bat              # 패키지 자동 설치
└── ...
```

### 설치 순서
1. 플러그인 폴더를 프로젝트 `Plugins` 폴더에 복사
2. `Binaries`, `Intermediate` 폴더 삭제 (있으면)
3. `install.bat` 더블클릭 (PyTorch, Transformers 등 자동 설치)
4. 언리얼 에디터 실행 → 플러그인 자동 빌드 + AI 서버 자동 시작

---

## GitHub

**Repository**: https://github.com/Bellcow41/AI_ControlRig

---

*Last Updated: 2026-01-15*
