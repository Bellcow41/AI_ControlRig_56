# 🦴 Bone Mapping AI - 완전 가이드 (A to Z)

## 📋 목차
1. [개요](#1-개요)
2. [하드웨어 요구사항](#2-하드웨어-요구사항)
3. [Phase 1: 환경 설정](#3-phase-1-환경-설정)
4. [Phase 2: 데이터 준비](#4-phase-2-데이터-준비)
5. [Phase 3: 파인튜닝](#5-phase-3-파인튜닝)
6. [Phase 4: 추론 및 통합](#6-phase-4-추론-및-통합)
7. [문제 해결](#7-문제-해결)

---

## 1. 개요

### 목표
스켈레탈 메쉬의 본 이름을 보고 AI가 자동으로 타겟 스켈레톤(UE5 Mannequin 등)에 매핑

### 작동 방식
```
입력: Mixamo 본 이름 리스트
       ["mixamorig:Hips", "mixamorig:Spine", "mixamorig:LeftArm", ...]
                              ↓
                    AI 모델 (Qwen 2.5 + LoRA)
                              ↓
출력: 매핑 결과
       {
         "mixamorig:Hips": "pelvis",
         "mixamorig:Spine": "spine_01",
         "mixamorig:LeftArm": "upperarm_l",
         ...
       }
```

---

## 2. 하드웨어 요구사항

### 최소 사양
| 항목 | 최소 | 권장 |
|------|------|------|
| GPU | RTX 3080 (10GB) | RTX 4090 (24GB) |
| RAM | 16GB | 32GB |
| Storage | 30GB | 50GB |
| CUDA | 11.8 | 12.1+ |

### GPU VRAM별 추천 모델
- **10-12GB**: Qwen2.5-7B-4bit (배치 사이즈 1)
- **24GB**: Qwen2.5-7B-4bit (배치 사이즈 4) 또는 14B-4bit
- **48GB+**: Qwen2.5-32B 또는 70B

---

## 3. Phase 1: 환경 설정

### Step 1.1: CUDA 설치 확인
```powershell
# CUDA 버전 확인
nvcc --version

# 없으면 설치: https://developer.nvidia.com/cuda-downloads
```

### Step 1.2: Python 환경 설정
```powershell
# 프로젝트 폴더로 이동
cd E:\AI\AI_ControlRig_02\BoneMapping_AI

# 자동 설정 (권장)
.\setup_environment.bat

# 또는 수동 설정
python -m venv venv
.\venv\Scripts\activate
pip install --upgrade pip
```

### Step 1.3: PyTorch + CUDA 설치
```powershell
# CUDA 12.1용
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121

# CUDA 11.8용
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
```

### Step 1.4: 패키지 설치
```powershell
pip install -r requirements.txt

# Unsloth (빠른 파인튜닝)
pip install "unsloth[colab-new] @ git+https://github.com/unslothai/unsloth.git"
```

### Step 1.5: 설치 확인
```python
import torch
print(f"PyTorch: {torch.__version__}")
print(f"CUDA Available: {torch.cuda.is_available()}")
print(f"GPU: {torch.cuda.get_device_name(0)}")

from unsloth import FastLanguageModel
print("Unsloth: OK")
```

---

## 4. Phase 2: 데이터 준비

### Step 2.1: 언리얼에서 본 데이터 추출

언리얼 에디터 Python 콘솔에서:
```python
import sys
sys.path.append("E:/AI/AI_ControlRig_02/BoneMapping_AI/01_data_collection")
import extract_bones_ue as extractor

# 단일 스켈레톤 추출
extractor.extract_single_skeleton("/Game/Characters/Mannequins/Meshes/SKM_Quinn")

# 프로젝트 전체 추출
extractor.batch_extract_all_skeletons(
    "E:/AI/AI_ControlRig_02/BoneMapping_AI/01_data_collection/raw_data"
)
```

### Step 2.2: 매핑 데이터 수집

이미 존재하는 매핑 데이터 활용:
- IK Retargeter 설정에서 추출
- 수작업으로 작성
- `examples/sample_mappings/standard_mappings.json` 참고

### Step 2.3: 학습 데이터셋 생성

```powershell
cd E:\AI\AI_ControlRig_02\BoneMapping_AI

python 02_data_processing/create_dataset.py `
    --input examples/sample_mappings/standard_mappings.json `
    --output 02_data_processing/processed_data/dataset.json `
    --split
```

생성되는 파일:
- `train.json`: 학습 데이터 (90%)
- `validation.json`: 검증 데이터 (10%)

### 데이터 형식 예시
```json
{
  "instruction": "Map the bones from the source skeleton to UE5 Mannequin",
  "input": "Source: Mixamo\nBones: mixamorig:Hips, mixamorig:Spine...",
  "output": "mixamorig:Hips -> pelvis\nmixamorig:Spine -> spine_01...",
  "system": "You are a bone mapping expert..."
}
```

---

## 5. Phase 3: 파인튜닝

### Step 3.1: 학습 설정 확인

`03_fine_tuning/train.py`에서 설정 조정:
```python
CONFIG = {
    "model_name": "unsloth/Qwen2.5-Coder-7B-Instruct-bnb-4bit",
    "lora_r": 16,           # LoRA rank (8/16/32)
    "batch_size": 2,        # VRAM에 따라 조절
    "num_epochs": 3,        # 에폭 수
    "learning_rate": 2e-4,  # 학습률
}
```

### Step 3.2: 학습 실행

```powershell
cd E:\AI\AI_ControlRig_02\BoneMapping_AI
.\venv\Scripts\activate

python 03_fine_tuning/train.py
```

### 예상 시간
| GPU | 데이터 100개 | 데이터 1000개 |
|-----|-------------|---------------|
| RTX 3090 | ~30분 | ~3시간 |
| RTX 4090 | ~15분 | ~1.5시간 |

### Step 3.3: 학습 모니터링

```
Training Progress:
  Epoch 1/3: ████████████░░░░ 75% | Loss: 0.85
  Epoch 2/3: ████████████████ 100% | Loss: 0.42
  ...
```

### Step 3.4: 모델 저장 확인

학습 완료 후:
```
03_fine_tuning/
└── checkpoints/
    └── bone_mapping_lora/
        ├── adapter_config.json
        ├── adapter_model.safetensors
        └── tokenizer files...
```

---

## 6. Phase 4: 추론 및 통합

### 방법 A: API 서버 사용 (권장)

#### A.1: 서버 실행
```powershell
cd E:\AI\AI_ControlRig_02\BoneMapping_AI\04_inference
python api_server.py

# 출력:
# 🚀 Server running at http://localhost:8000
```

#### A.2: 테스트
```powershell
# curl 또는 Postman으로 테스트
curl -X POST http://localhost:8000/predict `
  -H "Content-Type: application/json" `
  -d '{"source_bones": ["mixamorig:Hips", "mixamorig:Spine"], "source_type": "Mixamo"}'
```

#### A.3: 언리얼에서 사용
```python
# 언리얼 에디터 Python 콘솔
import sys
sys.path.append("E:/AI/AI_ControlRig_02/BoneMapping_AI/04_inference")
import ue_integration

# 자동 매핑
mapping = ue_integration.ai_bone_mapping(
    "/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon"
)

# 결과 확인
for src, tgt in mapping.items():
    print(f"{src} -> {tgt}")
```

### 방법 B: 직접 추론

```powershell
python 04_inference/inference.py `
  --model 03_fine_tuning/checkpoints/bone_mapping_lora `
  --source-bones "mixamorig:Hips" "mixamorig:Spine" "mixamorig:LeftArm" `
  --source-type Mixamo `
  --output result.json
```

---

## 7. 문제 해결

### ❌ CUDA Out of Memory
```
해결:
1. batch_size 줄이기 (2 → 1)
2. max_seq_length 줄이기 (2048 → 1024)
3. gradient_accumulation_steps 늘리기
```

### ❌ Unsloth 설치 실패
```powershell
# 대안: PEFT 직접 사용
pip install peft transformers accelerate bitsandbytes
```

### ❌ 모델 로드 실패
```
해결:
1. Hugging Face 로그인: huggingface-cli login
2. 모델 접근 권한 확인
3. 인터넷 연결 확인 (첫 다운로드 시 필요)
```

### ❌ 매핑 정확도가 낮음
```
해결:
1. 학습 데이터 늘리기 (최소 100개 매핑 쌍)
2. 에폭 수 늘리기 (3 → 5)
3. 데이터 증강 활성화
4. 더 큰 모델 사용 (7B → 14B)
```

---

## 📊 전체 워크플로우 요약

```
1. 환경 설정
   └── setup_environment.bat 실행

2. 데이터 준비
   ├── 언리얼에서 본 추출
   ├── 매핑 데이터 작성/수집
   └── create_dataset.py 실행

3. 파인튜닝
   └── python train.py (1-3시간)

4. 추론
   ├── API 서버 시작
   └── 언리얼에서 ai_bone_mapping() 호출

5. 결과 적용
   └── IK Retargeter에 매핑 적용
```

---

## 📞 추가 지원

문제가 있으면 다음을 확인:
1. GPU 드라이버 최신 버전인지
2. Python 3.10+ 사용 중인지
3. 가상환경이 활성화되었는지
4. 데이터 형식이 올바른지

Happy Fine-tuning! 🚀















