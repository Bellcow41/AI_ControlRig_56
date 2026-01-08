# AI SetUp Tool 56 V1 (UE 5.6 Plugin)

> AI 기반 본 매핑으로 Control Rig, IK Rig, Kawaii Physics AnimBP, Physics Asset 자동 생성

## 설치 방법

### 1. 플러그인 복사
```
[프로젝트]/Plugins/AI_SetUpTool_56_V1/
```

### 2. Python 환경 설치 (처음 한 번만)
```bash
# 더블클릭 또는 명령어 실행
install.bat
```

### 3. 에디터 실행
- 에디터 시작 시 플러그인 자동 빌드 (Binaries 없는 경우)
- AI 서버 자동 시작

## 사용 방법

**Tools → AI Control Rig Tool**

| 탭 | 기능 |
|----|------|
| Control Rig | AI 본 매핑 + Control Rig 생성 |
| IK Rig | IK Rig + Retargeter 생성 |
| Kawaii Physics | AnimBP 자동 생성 |
| Physics Asset | 캡슐 콜리전 자동 생성 |

## 포함된 파일

```
AI_SetUpTool_56_V1/
├── Source/AI_SetUpTool_56_V1/  # C++ 소스
├── BoneMapping_AI/
│   ├── python/                  # Python 3.11 임베디드
│   ├── 03_fine_tuning/          # LoRA 모델
│   └── 04_inference/            # API 서버
├── Resources/                   # 템플릿 Control Rig
└── install.bat                  # Python 패키지 설치
```

## 5.6 vs 5.7 차이점

| 항목 | 5.6 (이 버전) | 5.7 |
|------|---------------|-----|
| 플러그인 이름 | `AI_SetUpTool_56_V1` | `AI_SetUpTool_57` |
| ControlRigBlueprint | `ControlRigBlueprint.h` | `ControlRigBlueprintLegacy.h` |
| CalcBoneVertInfos | 3개 인자 | 4개 인자 |

## 배포 시 주의

다른 컴퓨터로 옮길 때:
1. `Binaries`, `Intermediate` 폴더 삭제
2. `install.bat` 실행 (PyTorch, Transformers 설치)
3. 에디터 실행 → 자동 빌드

---

*Last Updated: 2025-01-06*
