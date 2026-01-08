"""
🦴 Bone Mapping AI - 언리얼 엔진 통합

학습된 AI 모델을 언리얼 에디터에서 사용하는 스크립트
이 파일은 언리얼 에디터의 Python 환경에서 실행됩니다.

사용 방법:
1. API 서버 방식 (권장): 별도 프로세스에서 API 서버 실행 후 HTTP 호출
2. 직접 로드 방식: 언리얼 Python에 모델 로드 (메모리 많이 사용)
"""

import unreal
import json
import os
from typing import Dict, List, Optional

# HTTP 요청을 위한 모듈 (언리얼 내장 또는 pip 설치 필요)
try:
    import urllib.request
    import urllib.error
    HAS_URLLIB = True
except ImportError:
    HAS_URLLIB = False


# ============================================================
# 설정
# ============================================================

AI_SERVER_URL = "http://localhost:8000"
AI_PREDICT_ENDPOINT = f"{AI_SERVER_URL}/predict"


# ============================================================
# API 서버 통신
# ============================================================

def call_ai_server(
    source_bones: List[str],
    target_bones: List[str] = None,
    source_type: str = "Unknown",
    target_type: str = "UE5_Mannequin"
) -> Optional[Dict[str, str]]:
    """
    AI 서버에 본 매핑 요청
    
    Returns:
        매핑 딕셔너리 또는 None (실패시)
    """
    
    if not HAS_URLLIB:
        unreal.log_error("urllib not available")
        return None
    
    payload = {
        "source_bones": source_bones,
        "target_bones": target_bones,
        "source_type": source_type,
        "target_type": target_type
    }
    
    try:
        data = json.dumps(payload).encode('utf-8')
        request = urllib.request.Request(
            AI_PREDICT_ENDPOINT,
            data=data,
            headers={'Content-Type': 'application/json'}
        )
        
        with urllib.request.urlopen(request, timeout=30) as response:
            result = json.loads(response.read().decode('utf-8'))
            return result.get("mapping", {})
            
    except urllib.error.URLError as e:
        unreal.log_error(f"AI Server connection failed: {e}")
        return None
    except Exception as e:
        unreal.log_error(f"AI prediction failed: {e}")
        return None


# ============================================================
# 스켈레톤 본 추출
# ============================================================

def get_skeleton_bones(skeletal_mesh_path: str) -> List[str]:
    """스켈레탈 메쉬에서 본 이름 추출"""
    
    skeletal_mesh = unreal.EditorAssetLibrary.load_asset(skeletal_mesh_path)
    
    if not skeletal_mesh:
        unreal.log_error(f"Failed to load: {skeletal_mesh_path}")
        return []
    
    skeleton = skeletal_mesh.get_editor_property('skeleton')
    if not skeleton:
        unreal.log_error(f"No skeleton found")
        return []
    
    bones = []
    
    # 본 이름 추출 시도
    try:
        for i in range(500):  # 최대 500개 본
            try:
                bone_name = skeleton.get_bone_name(i)
                if bone_name and str(bone_name) != "None":
                    bones.append(str(bone_name))
                else:
                    break
            except:
                break
    except Exception as e:
        unreal.log_warning(f"Bone extraction method 1 failed: {e}")
    
    return bones


def get_ik_rig_bones(ik_rig_path: str) -> List[str]:
    """IK Rig에서 본 이름 추출"""
    
    ik_rig = unreal.EditorAssetLibrary.load_asset(ik_rig_path)
    
    if not ik_rig:
        unreal.log_error(f"Failed to load IK Rig: {ik_rig_path}")
        return []
    
    # IK Rig에서 본 정보 추출
    bones = []
    
    try:
        # 스켈레톤 프리뷰 메쉬에서 추출
        preview_mesh = ik_rig.get_editor_property('preview_skeletal_mesh')
        if preview_mesh:
            skeleton = preview_mesh.get_editor_property('skeleton')
            if skeleton:
                for i in range(500):
                    try:
                        bone_name = skeleton.get_bone_name(i)
                        if bone_name and str(bone_name) != "None":
                            bones.append(str(bone_name))
                        else:
                            break
                    except:
                        break
    except Exception as e:
        unreal.log_warning(f"IK Rig bone extraction failed: {e}")
    
    return bones


# ============================================================
# AI 기반 본 매핑
# ============================================================

def ai_bone_mapping(
    source_mesh_path: str,
    target_mesh_path: str = None,
    source_type: str = None,
    target_type: str = "UE5_Mannequin"
) -> Dict[str, str]:
    """
    AI를 사용한 자동 본 매핑
    
    Args:
        source_mesh_path: 소스 스켈레탈 메쉬 경로
        target_mesh_path: 타겟 스켈레탈 메쉬 경로 (None이면 UE5 기본 사용)
        source_type: 소스 스켈레톤 타입 (자동 감지 시도)
        target_type: 타겟 스켈레톤 타입
        
    Returns:
        본 매핑 딕셔너리
    """
    
    unreal.log("🦴 Starting AI Bone Mapping...")
    
    # 소스 본 추출
    source_bones = get_skeleton_bones(source_mesh_path)
    if not source_bones:
        unreal.log_error("Failed to extract source bones")
        return {}
    
    unreal.log(f"  Source bones: {len(source_bones)}")
    
    # 타겟 본 추출 (선택사항)
    target_bones = None
    if target_mesh_path:
        target_bones = get_skeleton_bones(target_mesh_path)
        unreal.log(f"  Target bones: {len(target_bones)}")
    
    # 소스 타입 자동 감지
    if source_type is None:
        source_type = _detect_skeleton_type(source_bones)
        unreal.log(f"  Detected source type: {source_type}")
    
    # AI 서버 호출
    unreal.log("  Calling AI server...")
    
    mapping = call_ai_server(
        source_bones=source_bones,
        target_bones=target_bones,
        source_type=source_type,
        target_type=target_type
    )
    
    if mapping:
        unreal.log(f"✅ AI Mapping complete: {len(mapping)} bones mapped")
    else:
        unreal.log_error("❌ AI Mapping failed")
        mapping = {}
    
    return mapping


def _detect_skeleton_type(bones: List[str]) -> str:
    """본 이름 패턴으로 스켈레톤 타입 감지"""
    
    bones_lower = [b.lower() for b in bones]
    bones_str = " ".join(bones_lower)
    
    # 패턴 매칭
    if "mixamorig" in bones_str:
        return "Mixamo"
    elif "bip001" in bones_str or "bip01" in bones_str:
        return "3dsMax_Biped"
    elif "deform" in bones_str and any("." in b for b in bones):
        return "Blender_Rigify"
    elif any(b.endswith("_l") or b.endswith("_r") for b in bones_lower):
        return "UE_Style"
    elif "hips" in bones_lower and "leftupleg" in bones_lower:
        return "Maya_HumanIK"
    else:
        return "Unknown"


# ============================================================
# IK Retargeter 적용
# ============================================================

def apply_mapping_to_retargeter(
    retargeter_path: str,
    mapping: Dict[str, str]
) -> bool:
    """
    본 매핑을 IK Retargeter에 적용
    
    Note: 이 기능은 언리얼 에디터 API 제한으로 완전 자동화가 어려울 수 있음
          수동 확인 권장
    """
    
    retargeter = unreal.EditorAssetLibrary.load_asset(retargeter_path)
    
    if not retargeter:
        unreal.log_error(f"Failed to load retargeter: {retargeter_path}")
        return False
    
    # TODO: IK Retargeter 체인 매핑 API 사용
    # 현재 언리얼 Python API가 제한적이므로
    # 매핑 결과를 파일로 저장하고 수동 적용 권장
    
    unreal.log_warning("Auto-apply to retargeter not fully implemented")
    unreal.log("Please apply mapping manually or use the exported JSON")
    
    return True


def export_mapping_for_manual_apply(
    mapping: Dict[str, str],
    output_path: str
) -> bool:
    """매핑 결과를 JSON으로 내보내기 (수동 적용용)"""
    
    try:
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump({
                "mapping": mapping,
                "format": "source_bone -> target_bone"
            }, f, indent=2, ensure_ascii=False)
        
        unreal.log(f"✅ Mapping exported to: {output_path}")
        return True
        
    except Exception as e:
        unreal.log_error(f"Failed to export mapping: {e}")
        return False


# ============================================================
# 메인 워크플로우
# ============================================================

def auto_bone_mapping_workflow(
    source_mesh_path: str,
    target_mesh_path: str = None,
    output_json_path: str = None
):
    """
    전체 자동 본 매핑 워크플로우
    
    사용 예:
        auto_bone_mapping_workflow(
            "/Game/Characters/MyChar/SKM_MyChar",
            output_json_path="D:/bone_mapping_result.json"
        )
    """
    
    unreal.log("=" * 60)
    unreal.log("🦴 AI Bone Mapping Workflow")
    unreal.log("=" * 60)
    
    # 1. AI 매핑 수행
    mapping = ai_bone_mapping(source_mesh_path, target_mesh_path)
    
    if not mapping:
        unreal.log_error("Workflow failed: No mapping generated")
        return None
    
    # 2. 결과 출력
    unreal.log("\n📋 Mapping Results:")
    for source, target in sorted(mapping.items()):
        unreal.log(f"  {source} -> {target}")
    
    # 3. JSON 내보내기
    if output_json_path:
        export_mapping_for_manual_apply(mapping, output_json_path)
    
    unreal.log("\n" + "=" * 60)
    unreal.log("✅ Workflow Complete!")
    unreal.log("=" * 60)
    
    return mapping


# ============================================================
# 사용 예시 (언리얼 에디터 Python 콘솔에서)
# ============================================================
"""
# 1. 먼저 별도 터미널에서 AI 서버 실행:
#    python api_server.py

# 2. 언리얼 에디터에서:

import importlib
from BoneMapping_AI.inference import ue_integration
importlib.reload(ue_integration)

# 단일 메쉬 매핑
mapping = ue_integration.ai_bone_mapping(
    "/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon"
)

# 결과 확인
for src, tgt in mapping.items():
    print(f"{src} -> {tgt}")

# 전체 워크플로우
ue_integration.auto_bone_mapping_workflow(
    "/Game/Characters/MyChar/SKM_MyChar",
    output_json_path="E:/AI/AI_ControlRig_02/bone_mapping_result.json"
)
"""















