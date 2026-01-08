"""
Bone Mapping API Server v4.0
체인 기반 분석 + 다중 신호 종합
"""
import os
import re
import json
from typing import Dict, List, Optional, Set, Tuple
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn

# ============================================================================
# UE5 표준 본 이름
# ============================================================================
UE5_BONES = {
    'root', 'pelvis',
    'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05',
    'neck_01', 'neck_02', 'head',
    'clavicle_l', 'clavicle_r',
    'upperarm_l', 'upperarm_r', 'lowerarm_l', 'lowerarm_r',
    'hand_l', 'hand_r',
    'thigh_l', 'thigh_r', 'calf_l', 'calf_r',
    'foot_l', 'foot_r', 'ball_l', 'ball_r',
    'thumb_01_l', 'thumb_02_l', 'thumb_03_l',
    'thumb_01_r', 'thumb_02_r', 'thumb_03_r',
    'index_01_l', 'index_02_l', 'index_03_l',
    'index_01_r', 'index_02_r', 'index_03_r',
    'middle_01_l', 'middle_02_l', 'middle_03_l',
    'middle_01_r', 'middle_02_r', 'middle_03_r',
    'ring_01_l', 'ring_02_l', 'ring_03_l',
    'ring_01_r', 'ring_02_r', 'ring_03_r',
    'pinky_01_l', 'pinky_02_l', 'pinky_03_l',
    'pinky_01_r', 'pinky_02_r', 'pinky_03_r',
}

# 세컨더리 본 키워드
SECONDARY_KEYWORDS = [
    'skirt', 'cape', 'cloak', 'cloth', 'ribbon', 'tassel',
    'hair', 'ponytail', 'pigtail', 'breast', 'boob',
    'weapon', 'attach', 'socket', 'slot', 'mount',
    'ik_', '_ik', 'ikgoal', 'ikpole', 'ctrl', 'control', 'helper',
    'twist', 'roll', '_tw', 'nub', '_end', 'dummy', '_dm_', '_ph_', '_b_',
    'point_', 'lookat', 'aim_', 'extra', 'aux_', 'sub_', 'add_',
]

# 표준 체인 구조 (pelvis 기준)
STANDARD_CHAINS = {
    'spine': ['spine_01', 'spine_02', 'spine_03'],
    'neck': ['neck_01', 'head'],
    'arm_l': ['clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l'],
    'arm_r': ['clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r'],
    'leg_l': ['thigh_l', 'calf_l', 'foot_l', 'ball_l'],
    'leg_r': ['thigh_r', 'calf_r', 'foot_r', 'ball_r'],
}

# 키워드 → 본 타입
BONE_TYPE_KEYWORDS = {
    'pelvis': ['pelvis', 'hip', 'hips', 'waist'],
    'spine': ['spine', 'chest', 'ribcage'],
    'neck': ['neck'],
    'head': ['head'],
    'clavicle': ['clavicle', 'shoulder'],  # 'collar' 제거 - 액세서리 본과 혼동
    'upperarm': ['upperarm', 'upper_arm', 'uparm', 'bicep', 'humerus'],
    'lowerarm': ['forearm', 'fore_arm', 'lowerarm', 'lower_arm', 'radius'],
    'hand': ['hand', 'wrist', 'palm'],
    'thigh': ['thigh', 'upperleg', 'upper_leg', 'upleg', 'femur'],
    'calf': ['calf', 'shin', 'lowerleg', 'lower_leg', 'tibia'],
    'foot': ['foot', 'ankle'],
    'ball': ['ball', 'toe0', 'toes'],
    'finger': ['finger', 'thumb', 'index', 'middle', 'ring', 'pinky'],
}

# ============================================================================
# 체인 분석 함수
# ============================================================================
def get_chain_from_bone(bone_name: str, all_bones: Dict, max_depth: int = 10, chain_type: str = None) -> List[str]:
    """본에서 시작해서 자식 체인 추출 (체인 타입에 따라 소거법 적용)"""
    chain = [bone_name]
    current = bone_name
    
    # 체인 타입별 제외 키워드
    EXCLUDE_FROM_SPINE = ['clavicle', 'shoulder', 'upperarm', 'arm', 'thigh', 'leg']
    EXCLUDE_FROM_LEG = ['spine', 'neck', 'head', 'arm', 'hand']
    EXCLUDE_FROM_ARM = ['spine', 'neck', 'head', 'thigh', 'leg', 'foot']
    
    for _ in range(max_depth):
        children = all_bones.get(current, {}).get('children', [])
        # 1차 필터: 세컨더리 제외
        valid_children = [c for c in children if not is_secondary_bone(c)]
        
        if not valid_children:
            break
        
        # 2차 필터: 체인 타입에 따른 소거법
        if chain_type == 'spine':
            # spine 체인에서 팔/다리 분기 제외
            filtered = []
            for c in valid_children:
                c_lower = c.lower()
                is_excluded = any(ex in c_lower for ex in EXCLUDE_FROM_SPINE)
                if not is_excluded:
                    filtered.append(c)
            if filtered:
                valid_children = filtered
        
        elif chain_type == 'leg':
            filtered = []
            for c in valid_children:
                c_lower = c.lower()
                is_excluded = any(ex in c_lower for ex in EXCLUDE_FROM_LEG)
                if not is_excluded:
                    filtered.append(c)
            if filtered:
                valid_children = filtered
        
        if not valid_children:
            break
            
        chain.append(valid_children[0])
        current = valid_children[0]
    return chain

def get_parent_chain(bone_name: str, all_bones: Dict, max_depth: int = 10) -> List[str]:
    """본에서 루트까지 부모 체인 추출"""
    chain = [bone_name]
    current = bone_name
    for _ in range(max_depth):
        parent = all_bones.get(current, {}).get('parent')
        if not parent:
            break
        chain.insert(0, parent)
        current = parent
    return chain

def is_secondary_bone(name: str) -> bool:
    """세컨더리 본인지 확인"""
    name_lower = name.lower()
    for kw in SECONDARY_KEYWORDS:
        if kw in name_lower:
            return True
    return False

def detect_side(name: str) -> Optional[str]:
    """L/R 사이드 감지"""
    name_lower = name.lower()
    if any(x in name_lower for x in ['-l-', '_l_', '-l', '_l', 'left', '.l.', '.l', '_l_', 'l-', 'l_']):
        return 'l'
    elif any(x in name_lower for x in ['-r-', '_r_', '-r', '_r', 'right', '.r.', '.r', '_r_', 'r-', 'r_']):
        return 'r'
    return None

def get_bone_type_from_keyword(name: str) -> Optional[str]:
    """키워드에서 본 타입 추출"""
    name_lower = name.lower()
    for bone_type, keywords in BONE_TYPE_KEYWORDS.items():
        for kw in keywords:
            if kw in name_lower:
                return bone_type
    return None

def find_pelvis(all_bones: Dict, mapped_bones: Dict) -> Optional[str]:
    """pelvis 본 찾기"""
    for name in all_bones:
        name_lower = name.lower()
        if any(kw in name_lower for kw in ['pelvis', 'hip', 'hips']):
            if not is_secondary_bone(name):
                return name
    return None

def find_root(all_bones: Dict) -> Optional[str]:
    """root 본 찾기 (부모 없는 본)"""
    for name, info in all_bones.items():
        if info.get('parent') is None:
            return name
    return None

# ============================================================================
# 체인 매핑 함수
# ============================================================================
def map_chain(chain: List[str], standard_chain: List[str], side: Optional[str]) -> Dict[str, str]:
    """체인을 표준 체인에 매핑"""
    mapping = {}
    
    # 체인 길이가 맞지 않으면 앞에서부터 순서대로 매핑
    for i, bone in enumerate(chain):
        if i < len(standard_chain):
            target = standard_chain[i]
            # 사이드 적용
            if side and not target.endswith(f'_{side}'):
                if target.endswith('_l') or target.endswith('_r'):
                    target = target[:-2] + f'_{side}'
            mapping[target] = bone
    
    return mapping

def analyze_and_map_skeleton(all_bones: Dict) -> Dict[str, str]:
    """전체 스켈레톤 분석 및 매핑"""
    final_mapping = {}
    mapped_sources = set()
    
    print("\n[Chain Analysis] Starting...")
    
    # 1. Root 찾기
    root = find_root(all_bones)
    if root:
        final_mapping['root'] = root
        mapped_sources.add(root)
        print(f"  Root: {root}")
    
    # 2. Pelvis 찾기
    pelvis = find_pelvis(all_bones, {})
    if pelvis:
        final_mapping['pelvis'] = pelvis
        mapped_sources.add(pelvis)
        print(f"  Pelvis: {pelvis}")
    else:
        print("  WARNING: Pelvis not found!")
        # Root의 첫 번째 자식을 pelvis로 시도
        if root:
            children = all_bones.get(root, {}).get('children', [])
            for c in children:
                if not is_secondary_bone(c):
                    pelvis = c
                    final_mapping['pelvis'] = pelvis
                    mapped_sources.add(pelvis)
                    print(f"  Pelvis (inferred from root child): {pelvis}")
                    break
    
    if not pelvis:
        return final_mapping
    
    # 3. Pelvis의 자식들 분석
    pelvis_children = all_bones.get(pelvis, {}).get('children', [])
    print(f"  Pelvis children: {pelvis_children}")
    
    spine_chain_start = None
    leg_chains_l = []
    leg_chains_r = []
    other_chains = []
    
    for child in pelvis_children:
        if is_secondary_bone(child):
            print(f"    {child}: SKIP (secondary)")
            continue
        
        child_chain = get_chain_from_bone(child, all_bones)
        child_type = get_bone_type_from_keyword(child)
        child_side = detect_side(child)
        
        print(f"    {child}: type={child_type}, side={child_side}, chain_len={len(child_chain)}")
        
        # Spine 체인
        if child_type == 'spine':
            spine_chain_start = child
        # 다리 체인 후보
        elif child_type == 'thigh' or (child_type is None and len(child_chain) >= 3):
            if child_side == 'l':
                leg_chains_l.append((child, child_chain))
            elif child_side == 'r':
                leg_chains_r.append((child, child_chain))
            else:
                other_chains.append((child, child_chain))
    
    # 4. Spine 체인 매핑 (소거법: 팔/다리 분기 제외)
    if spine_chain_start:
        spine_chain = get_chain_from_bone(spine_chain_start, all_bones, chain_type='spine')
        print(f"\n  Spine chain: {spine_chain}")
        
        spine_index = 1  # spine_01, spine_02, spine_03...
        for i, bone in enumerate(spine_chain):
            bone_lower = bone.lower()
            
            # 3dsMax Biped 규칙: Spine, Spine1, Spine2 → spine_01, spine_02, spine_03
            if 'spine2' in bone_lower:
                final_mapping['spine_03'] = bone
                mapped_sources.add(bone)
            elif 'spine1' in bone_lower:
                final_mapping['spine_02'] = bone
                mapped_sources.add(bone)
            elif 'spine' in bone_lower:
                # 위치 기반 매핑 (SpineA, SpineB 같은 경우)
                target = f'spine_0{spine_index}'
                if spine_index <= 5 and target not in final_mapping:
                    final_mapping[target] = bone
                    mapped_sources.add(bone)
                    print(f"    {bone} -> {target}")
                    spine_index += 1
            elif 'neck' in bone_lower:
                final_mapping['neck_01'] = bone
                mapped_sources.add(bone)
            elif 'head' in bone_lower:
                final_mapping['head'] = bone
                mapped_sources.add(bone)
    
    # 5. 다리 체인 매핑
    def map_leg_chain(chains: List, side: str):
        if not chains:
            return
        
        # 가장 긴 체인 선택 (또는 thigh 키워드가 있는 체인)
        best_chain = None
        best_score = -1
        
        for start, chain in chains:
            score = len(chain)
            if get_bone_type_from_keyword(start) == 'thigh':
                score += 100  # thigh 키워드 보너스
            if score > best_score:
                best_score = score
                best_chain = (start, chain)
        
        if best_chain:
            start, chain = best_chain
            print(f"\n  Leg chain ({side}): {chain}")
            
            leg_targets = [f'thigh_{side}', f'calf_{side}', f'foot_{side}', f'ball_{side}']
            
            for i, bone in enumerate(chain):
                if i < len(leg_targets):
                    target = leg_targets[i]
                    if target not in final_mapping:
                        final_mapping[target] = bone
                        mapped_sources.add(bone)
                        print(f"    {bone} -> {target}")
    
    map_leg_chain(leg_chains_l, 'l')
    map_leg_chain(leg_chains_r, 'r')
    
    # 사이드 없는 체인들 - L/R 추론 시도
    if other_chains and (not leg_chains_l or not leg_chains_r):
        for start, chain in other_chains:
            # 위치 기반 또는 이름 기반 L/R 추론
            if not leg_chains_l and 'thigh_l' not in final_mapping:
                print(f"\n  Assuming leg chain (l): {chain}")
                map_leg_chain([(start, chain)], 'l')
                leg_chains_l.append((start, chain))
            elif not leg_chains_r and 'thigh_r' not in final_mapping:
                print(f"\n  Assuming leg chain (r): {chain}")
                map_leg_chain([(start, chain)], 'r')
    
    # 6. 팔 체인 매핑 - hand 본을 먼저 찾고 부모 체인 추적
    print(f"\n  [Arm Chain] Finding hand bones first...")
    
    for side in ['l', 'r']:
        # 1. hand 본 찾기
        hand_bone = None
        for name in all_bones:
            if is_secondary_bone(name):
                continue
            name_lower = name.lower()
            bone_side = detect_side(name)
            if bone_side == side and 'hand' in name_lower:
                hand_bone = name
                break
        
        if not hand_bone:
            print(f"    Hand ({side}): NOT FOUND")
            continue
        
        print(f"    Hand ({side}): {hand_bone}")
        
        # 2. hand에서 부모 체인 거슬러 올라가기
        parent_chain = get_parent_chain(hand_bone, all_bones)
        print(f"    Parent chain: {parent_chain}")
        
        # 3. 체인에서 팔 본들 식별 (뒤에서부터: hand → lowerarm → upperarm → clavicle)
        arm_bones = []
        for bone in reversed(parent_chain):
            bone_type = get_bone_type_from_keyword(bone)
            if bone_type in ['hand', 'lowerarm', 'upperarm', 'clavicle']:
                arm_bones.insert(0, bone)
            elif bone_type in ['spine', 'neck', 'head', 'pelvis']:
                break  # spine 도달하면 중지
            elif len(arm_bones) > 0:
                # 이미 팔 본이 있으면 이름 없어도 체인에 추가
                arm_bones.insert(0, bone)
            
            if len(arm_bones) >= 4:  # clavicle, upperarm, lowerarm, hand
                break
        
        print(f"    Arm bones: {arm_bones}")
        
        # 4. 매핑 적용 (뒤에서부터 매핑)
        if len(arm_bones) >= 3:
            # 마지막 3개: upperarm, lowerarm, hand
            if f'hand_{side}' not in final_mapping:
                final_mapping[f'hand_{side}'] = arm_bones[-1]
                mapped_sources.add(arm_bones[-1])
            if f'lowerarm_{side}' not in final_mapping:
                final_mapping[f'lowerarm_{side}'] = arm_bones[-2]
                mapped_sources.add(arm_bones[-2])
            if f'upperarm_{side}' not in final_mapping:
                final_mapping[f'upperarm_{side}'] = arm_bones[-3]
                mapped_sources.add(arm_bones[-3])
            if len(arm_bones) >= 4 and f'clavicle_{side}' not in final_mapping:
                final_mapping[f'clavicle_{side}'] = arm_bones[-4]
                mapped_sources.add(arm_bones[-4])
    
    # 7. 손가락 매핑 (hand 자식에서 찾기)
    for side in ['l', 'r']:
        hand = final_mapping.get(f'hand_{side}')
        if hand:
            hand_children = all_bones.get(hand, {}).get('children', [])
            map_fingers(hand_children, all_bones, side, final_mapping, mapped_sources)
    
    # 8. 나머지 키워드 기반 매핑
    for name, info in all_bones.items():
        if name in mapped_sources:
            continue
        if is_secondary_bone(name):
            continue
        
        bone_type = get_bone_type_from_keyword(name)
        side = detect_side(name)
        
        if bone_type and side:
            target = f"{bone_type}_{side}"
            if target in UE5_BONES and target not in final_mapping:
                final_mapping[target] = name
                mapped_sources.add(name)
                print(f"  [Keyword] {name} -> {target}")
    
    return final_mapping

def map_fingers(finger_roots: List[str], all_bones: Dict, side: str,
                mapping: Dict, mapped: Set):
    """손가락 매핑"""
    finger_names = ['thumb', 'index', 'middle', 'ring', 'pinky']

    for root in finger_roots:
        if is_secondary_bone(root):
            continue

        root_lower = root.lower()
        chain = get_chain_from_bone(root, all_bones)

        # 손가락 타입 감지
        finger_type = None
        for fname in finger_names:
            if fname in root_lower:
                finger_type = fname
                break
    
        # Biped 형식: Finger0 = thumb, Finger1 = index, etc.
        if 'finger0' in root_lower:
            finger_type = 'thumb'
        elif 'finger1' in root_lower:
            finger_type = 'index'
        elif 'finger2' in root_lower:
            finger_type = 'middle'
        elif 'finger3' in root_lower:
            finger_type = 'ring'
        elif 'finger4' in root_lower:
            finger_type = 'pinky'

        if finger_type:
            targets = [f'{finger_type}_01_{side}', f'{finger_type}_02_{side}', f'{finger_type}_03_{side}']
            for i, bone in enumerate(chain):
                if i < len(targets):
                    target = targets[i]
                    if target not in mapping:
                        mapping[target] = bone
                        mapped.add(bone)

# ============================================================================
# API
# ============================================================================
class BoneInfo(BaseModel):
    name: str
    parent: Optional[str] = None
    children: Optional[List[str]] = None

class MappingRequest(BaseModel):
    bones: List[BoneInfo]
    use_ai: bool = True

class MappingResponse(BaseModel):
    mapping: Dict[str, str]
    method: str
    bone_count: int

class ApproveRequest(BaseModel):
    skeleton_name: str
    bones: List[BoneInfo]
    mapping: Dict[str, str]

class ApproveResponse(BaseModel):
    saved: bool
    total_samples: int
    auto_train_triggered: bool
    message: str

# 학습 데이터 저장 경로
APPROVED_DATA_PATH = os.path.join(os.path.dirname(__file__), "..", "02_data_prep", "approved_mappings.jsonl")
AUTO_TRAIN_THRESHOLD = 10  # 10개 쌓이면 자동 학습

app = FastAPI(title="Bone Mapping API v4.0", version="4.0.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

ai_inference = None
ai_loaded = False

@app.on_event("startup")
async def startup():
    global ai_inference, ai_loaded
    try:
        from inference import get_inference
        ai_inference = get_inference()
        ai_loaded = ai_inference.load_model()
        print(f"[Server] AI loaded: {ai_loaded}")
    except Exception as e:
        print(f"[Server] AI load failed: {e}")

@app.get("/health")
async def health():
    return {"status": "healthy", "ai_ready": ai_loaded}

@app.post("/predict", response_model=MappingResponse)
async def predict_mapping(request: MappingRequest):
    """체인 기반 분석으로 매핑"""
    
    # 본 정보 구축
    all_bones = {b.name: {"parent": b.parent, "children": b.children or []} for b in request.bones}
    
    print(f"\n{'='*60}")
    print(f"[Mapping] {len(request.bones)} bones")
    print(f"{'='*60}")
    
    # 체인 기반 분석
    final_mapping = analyze_and_map_skeleton(all_bones)
    
    print(f"\n[Result] {len(final_mapping)} mappings")
    for target, source in sorted(final_mapping.items()):
        print(f"  {target} <- {source}")
    
    return MappingResponse(
        mapping=final_mapping,
        method="chain_analysis_v4",
        bone_count=len(final_mapping)
    )

def count_approved_samples() -> int:
    """승인된 샘플 개수 카운트"""
    if not os.path.exists(APPROVED_DATA_PATH):
        return 0
    with open(APPROVED_DATA_PATH, 'r', encoding='utf-8') as f:
        return sum(1 for _ in f)

def trigger_auto_training():
    """백그라운드에서 자동 학습 실행"""
    import subprocess
    import threading
    
    def run_training():
        print("\n" + "="*60)
        print("[AUTO-TRAIN] Starting automatic fine-tuning...")
        print("="*60)
        
        train_script = os.path.join(os.path.dirname(__file__), "..", "03_fine_tuning", "train_incremental.py")
        
        try:
            result = subprocess.run(
                ["C:\\Users\\Administrator\\AppData\\Local\\Programs\\Python\\Python311\\python.exe", train_script],
                capture_output=True,
                text=True,
                cwd=os.path.dirname(train_script)
            )
            if result.returncode == 0:
                print("[AUTO-TRAIN] Training completed successfully!")
                print(result.stdout[-500:] if len(result.stdout) > 500 else result.stdout)
            else:
                print(f"[AUTO-TRAIN] Training failed: {result.stderr[-500:]}")
        except Exception as e:
            print(f"[AUTO-TRAIN] Error: {e}")
    
    # 백그라운드 스레드에서 실행
    thread = threading.Thread(target=run_training, daemon=True)
    thread.start()

@app.post("/approve", response_model=ApproveResponse)
async def approve_mapping(request: ApproveRequest):
    """매핑 승인 - 학습 데이터로 저장 + 자동 학습 트리거"""
    
    print(f"\n{'='*60}")
    print(f"[APPROVE] Skeleton: {request.skeleton_name}")
    print(f"[APPROVE] Mappings: {len(request.mapping)}")
    print(f"{'='*60}")
    
    # 본 정보 딕셔너리 구축
    bone_info = {b.name: {"parent": b.parent, "children": b.children or []} for b in request.bones}
    
    # 학습 데이터 형식으로 변환
    training_entries = []
    for ue5_bone, source_bone in request.mapping.items():
        if source_bone in bone_info:
            info = bone_info[source_bone]
            entry = {
                "input": f"Bone: {source_bone}, Parent: {info['parent']}, Children: {info['children']}",
                "output": ue5_bone
            }
            training_entries.append(entry)
    
    # 파일에 추가
    os.makedirs(os.path.dirname(APPROVED_DATA_PATH), exist_ok=True)
    with open(APPROVED_DATA_PATH, 'a', encoding='utf-8') as f:
        for entry in training_entries:
            f.write(json.dumps(entry, ensure_ascii=False) + '\n')
    
    total_samples = count_approved_samples()
    print(f"[APPROVE] Saved {len(training_entries)} entries. Total: {total_samples}")
    
    # 자동 학습 트리거 체크
    auto_train = False
    if total_samples >= AUTO_TRAIN_THRESHOLD and total_samples % AUTO_TRAIN_THRESHOLD == 0:
        print(f"[APPROVE] Threshold reached ({AUTO_TRAIN_THRESHOLD}). Triggering auto-training...")
        trigger_auto_training()
        auto_train = True
    
    return ApproveResponse(
        saved=True,
        total_samples=total_samples,
        auto_train_triggered=auto_train,
        message=f"Saved {len(training_entries)} mappings. Total: {total_samples}" + 
                (" Auto-training started!" if auto_train else f" ({AUTO_TRAIN_THRESHOLD - (total_samples % AUTO_TRAIN_THRESHOLD)} more for auto-train)")
    )

@app.get("/training_status")
async def training_status():
    """학습 데이터 상태 확인"""
    total = count_approved_samples()
    remaining = AUTO_TRAIN_THRESHOLD - (total % AUTO_TRAIN_THRESHOLD)
    return {
        "total_samples": total,
        "threshold": AUTO_TRAIN_THRESHOLD,
        "remaining_for_next_train": remaining if remaining != AUTO_TRAIN_THRESHOLD else 0
    }

# ============================================================================
# 본 분류 피드백 API (helper/secondary 분류 학습용)
# ============================================================================
CLASSIFICATION_DATA_PATH = os.path.join(os.path.dirname(__file__), "..", "02_data_prep", "bone_classification.jsonl")
CLASSIFICATION_TRAIN_THRESHOLD = 20  # 20개 쌓이면 자동 학습

class BoneClassificationFeedback(BaseModel):
    bone_name: str
    parent: Optional[str] = None
    children: Optional[List[str]] = None
    classification: str  # "helper", "secondary", "zero"
    skeleton_name: Optional[str] = None

class ClassificationFeedbackResponse(BaseModel):
    saved: bool
    total_samples: int
    message: str

def count_classification_samples() -> int:
    """분류 학습 샘플 개수 카운트"""
    if not os.path.exists(CLASSIFICATION_DATA_PATH):
        return 0
    with open(CLASSIFICATION_DATA_PATH, 'r', encoding='utf-8') as f:
        return sum(1 for _ in f)

@app.post("/classify_feedback", response_model=ClassificationFeedbackResponse)
async def classify_feedback(request: BoneClassificationFeedback):
    """본 분류 피드백 저장 - helper/secondary/zero 학습 데이터"""
    
    print(f"\n{'='*60}")
    print(f"[CLASSIFY] Bone: {request.bone_name}")
    print(f"[CLASSIFY] Classification: {request.classification}")
    print(f"{'='*60}")
    
    # 학습 데이터 형식
    entry = {
        "input": {
            "bone_name": request.bone_name,
            "parent": request.parent,
            "children": request.children or []
        },
        "output": request.classification,
        "skeleton": request.skeleton_name
    }
    
    # 파일에 추가
    os.makedirs(os.path.dirname(CLASSIFICATION_DATA_PATH), exist_ok=True)
    with open(CLASSIFICATION_DATA_PATH, 'a', encoding='utf-8') as f:
        f.write(json.dumps(entry, ensure_ascii=False) + '\n')
    
    total_samples = count_classification_samples()
    print(f"[CLASSIFY] Total samples: {total_samples}")
    
    # TODO: 일정 수 이상 모이면 분류 모델 학습 트리거
    message = f"Saved! Total: {total_samples}/{CLASSIFICATION_TRAIN_THRESHOLD}"
    if total_samples >= CLASSIFICATION_TRAIN_THRESHOLD:
        message += " - Ready for training!"
    
    return ClassificationFeedbackResponse(
        saved=True,
        total_samples=total_samples,
        message=message
    )

@app.get("/classification_status")
async def classification_status():
    """본 분류 학습 데이터 상태"""
    total = count_classification_samples()
    return {
        "total_samples": total,
        "threshold": CLASSIFICATION_TRAIN_THRESHOLD,
        "ready_for_training": total >= CLASSIFICATION_TRAIN_THRESHOLD
    }

@app.post("/classify_batch")
async def classify_batch(bones: List[BoneClassificationFeedback]):
    """여러 본 분류 피드백 일괄 저장"""
    
    print(f"\n{'='*60}")
    print(f"[CLASSIFY BATCH] {len(bones)} bones")
    print(f"{'='*60}")
    
    os.makedirs(os.path.dirname(CLASSIFICATION_DATA_PATH), exist_ok=True)
    
    with open(CLASSIFICATION_DATA_PATH, 'a', encoding='utf-8') as f:
        for bone in bones:
            entry = {
                "input": {
                    "bone_name": bone.bone_name,
                    "parent": bone.parent,
                    "children": bone.children or []
                },
                "output": bone.classification,
                "skeleton": bone.skeleton_name
            }
            f.write(json.dumps(entry, ensure_ascii=False) + '\n')
    
    total_samples = count_classification_samples()
    
    return {
        "saved": len(bones),
        "total_samples": total_samples,
        "message": f"Saved {len(bones)} bones. Total: {total_samples}"
    }

if __name__ == "__main__":
    print("="*60)
    print("Bone Mapping API v4.0")
    print("체인 기반 분석 + 다중 신호 종합")
    print("="*60)
    uvicorn.run(app, host="0.0.0.0", port=8000)
