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

## 플러그인 탭 구성

| 탭 | 기능 | 색상 테마 |
|----|------|-----------|
| **Control Rig** | AI 본 매핑 + Control Rig 자동 생성 | 파랑 |
| **IK Rig** | IK Rig + Retargeter 자동 생성 | 파랑 |
| **Kawaii Physics** | Secondary 본용 AnimBP 자동 생성 | 핑크 |
| **Physics Asset** | 메인 본 캡슐 콜리전 자동 생성 | 주황 |

## 프로젝트 구조

```
AI_SetUpTool_56/
├── Plugins/
│   └── AI_SetUpTool_56_V1/          # UE5.6 플러그인
│       ├── Source/AI_SetUpTool_56_V1/
│       │   ├── Public/
│       │   │   └── SControlRigToolWidget.h
│       │   └── Private/
│       │       ├── SControlRigToolWidget.cpp
│       │       └── ControlRigToolModule.cpp
│       ├── BoneMapping_AI/          # AI 시스템
│       │   ├── python/              # 임베디드 Python 3.11
│       │   ├── 03_fine_tuning/      # LoRA 모델
│       │   └── 04_inference/        # API 서버
│       ├── Resources/               # 템플릿 Control Rig
│       └── install.bat              # Python 환경 자동 설치
└── Content/
```

## 설치 및 실행

### 1. 플러그인 설치
1. `Plugins/AI_SetUpTool_56_V1` 폴더를 프로젝트의 `Plugins` 폴더에 복사
2. `Binaries`, `Intermediate` 폴더가 있으면 삭제 (첫 실행 시 자동 빌드)
3. `install.bat` 실행 (Python 패키지 자동 설치)
4. 언리얼 에디터 실행

### 2. 에디터에서 사용
1. **Tools → AI Control Rig Tool**
2. 원하는 탭 선택 (Control Rig / IK Rig / Kawaii Physics / Physics Asset)
3. 에셋 선택 후 생성 버튼 클릭

### 3. 수동 빌드 (개발용)
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

*Last Updated: 2025-01-06*

