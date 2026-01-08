"""
언리얼 에디터에서 스켈레탈 메쉬의 본 정보를 추출하는 스크립트
이 스크립트는 언리얼 에디터의 Python 콘솔에서 실행해야 합니다.
"""

import unreal
import json
import os
from datetime import datetime


def get_all_skeletal_meshes():
    """프로젝트의 모든 스켈레탈 메쉬 에셋을 찾습니다."""
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    
    # SkeletalMesh 클래스 필터
    skeletal_meshes = asset_registry.get_assets_by_class(
        unreal.TopLevelAssetPath("/Script/Engine", "SkeletalMesh")
    )
    
    return skeletal_meshes


def extract_bone_hierarchy(skeleton):
    """스켈레톤에서 본 계층 구조를 추출합니다."""
    bones = []
    
    bone_names = skeleton.get_editor_property('bone_tree')
    # 또는 직접 접근
    ref_skeleton = skeleton.get_editor_property('ref_skeleton') if hasattr(skeleton, 'ref_skeleton') else None
    
    # 본 정보 수집
    num_bones = skeleton.get_num_bones() if hasattr(skeleton, 'get_num_bones') else 0
    
    for i in range(num_bones):
        bone_name = skeleton.get_bone_name(i)
        parent_index = skeleton.get_parent_index(i)
        parent_name = skeleton.get_bone_name(parent_index) if parent_index >= 0 else "None"
        
        bones.append({
            "index": i,
            "name": str(bone_name),
            "parent_index": parent_index,
            "parent_name": str(parent_name)
        })
    
    return bones


def extract_skeleton_info(skeletal_mesh_path):
    """스켈레탈 메쉬에서 스켈레톤 정보를 추출합니다."""
    
    # 에셋 로드
    skeletal_mesh = unreal.EditorAssetLibrary.load_asset(skeletal_mesh_path)
    
    if not skeletal_mesh:
        unreal.log_error(f"Failed to load: {skeletal_mesh_path}")
        return None
    
    # 스켈레톤 가져오기
    skeleton = skeletal_mesh.get_editor_property('skeleton')
    
    if not skeleton:
        unreal.log_error(f"No skeleton found for: {skeletal_mesh_path}")
        return None
    
    # 본 정보 추출
    bones = []
    ref_skeleton = skeleton.get_editor_property('ref_skeleton')
    
    # USkeleton에서 본 정보 가져오기
    bone_tree = skeleton.get_bone_tree()
    
    for bone_info in bone_tree:
        bone_name = bone_info.get_editor_property('name')
        parent_index = bone_info.get_editor_property('parent_index')
        
        bones.append({
            "name": str(bone_name),
            "parent_index": parent_index
        })
    
    return {
        "asset_path": skeletal_mesh_path,
        "skeleton_name": str(skeleton.get_name()),
        "bone_count": len(bones),
        "bones": bones,
        "extracted_at": datetime.now().isoformat()
    }


def extract_skeleton_simple(skeletal_mesh_path):
    """간단한 방법으로 스켈레톤 본 리스트 추출 (더 안정적)"""
    
    skeletal_mesh = unreal.EditorAssetLibrary.load_asset(skeletal_mesh_path)
    
    if not skeletal_mesh:
        return None
    
    skeleton = skeletal_mesh.get_editor_property('skeleton')
    if not skeleton:
        return None
    
    # 본 이름만 추출
    bone_names = []
    
    # Method 1: RefSkeleton 사용
    try:
        # GetReferenceSkeleton()은 FReferenceSkeleton 반환
        for i in range(200):  # 최대 200개 본 가정
            try:
                bone_name = skeleton.get_bone_name(i)
                if bone_name and str(bone_name) != "None":
                    bone_names.append(str(bone_name))
                else:
                    break
            except:
                break
    except Exception as e:
        unreal.log_warning(f"Method 1 failed: {e}")
    
    # Method 2: 직접 순회
    if not bone_names:
        try:
            bone_tree = skeleton.get_editor_property('bone_tree')
            for bone in bone_tree:
                bone_names.append(str(bone.name))
        except Exception as e:
            unreal.log_warning(f"Method 2 failed: {e}")
    
    return {
        "asset_path": skeletal_mesh_path,
        "skeleton_name": skeleton.get_name(),
        "bones": bone_names,
        "bone_count": len(bone_names)
    }


def batch_extract_all_skeletons(output_dir="D:/BoneMappingData"):
    """프로젝트의 모든 스켈레톤 정보를 추출하여 저장"""
    
    os.makedirs(output_dir, exist_ok=True)
    
    # 모든 스켈레탈 메쉬 찾기
    skeletal_meshes = get_all_skeletal_meshes()
    
    results = []
    
    for asset_data in skeletal_meshes:
        asset_path = str(asset_data.get_editor_property('object_path'))
        
        unreal.log(f"Processing: {asset_path}")
        
        info = extract_skeleton_simple(asset_path)
        if info:
            results.append(info)
    
    # JSON으로 저장
    output_file = os.path.join(output_dir, f"skeletons_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json")
    
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=2, ensure_ascii=False)
    
    unreal.log(f"Saved {len(results)} skeletons to: {output_file}")
    
    return results


def extract_single_skeleton(skeletal_mesh_path):
    """단일 스켈레탈 메쉬의 본 정보 출력 (디버깅용)"""
    
    info = extract_skeleton_simple(skeletal_mesh_path)
    
    if info:
        print(f"\n=== {info['skeleton_name']} ===")
        print(f"Bone Count: {info['bone_count']}")
        print("\nBones:")
        for i, bone in enumerate(info['bones']):
            print(f"  [{i}] {bone}")
    else:
        print(f"Failed to extract: {skeletal_mesh_path}")
    
    return info


# ============================================================
# 사용 예시 (언리얼 에디터 Python 콘솔에서)
# ============================================================
"""
# 단일 스켈레톤 추출
import importlib
import BoneMapping_AI.01_data_collection.extract_bones_ue as extractor
importlib.reload(extractor)

extractor.extract_single_skeleton("/Game/Characters/Mannequins/Meshes/SKM_Quinn")

# 전체 추출
extractor.batch_extract_all_skeletons("E:/AI/AI_ControlRig_02/BoneMapping_AI/01_data_collection/raw_data")
"""


# ============================================================
# IK Retargeter에서 매핑 정보 추출
# ============================================================
def extract_ik_retargeter_mapping(retargeter_path):
    """IK Retargeter 에셋에서 본 매핑 정보를 추출합니다."""
    
    retargeter = unreal.EditorAssetLibrary.load_asset(retargeter_path)
    
    if not retargeter:
        unreal.log_error(f"Failed to load retargeter: {retargeter_path}")
        return None
    
    # 소스/타겟 IK Rig 가져오기
    source_rig = retargeter.get_editor_property('source_ik_rig_asset')
    target_rig = retargeter.get_editor_property('target_ik_rig_asset')
    
    if not source_rig or not target_rig:
        return None
    
    # 체인 매핑 정보 추출
    chain_mappings = retargeter.get_editor_property('chain_settings')
    
    mappings = []
    for chain in chain_mappings:
        source_chain = chain.get_editor_property('source_chain')
        target_chain = chain.get_editor_property('target_chain')
        
        mappings.append({
            "source": str(source_chain),
            "target": str(target_chain)
        })
    
    return {
        "retargeter_path": retargeter_path,
        "source_rig": str(source_rig.get_name()),
        "target_rig": str(target_rig.get_name()),
        "chain_mappings": mappings
    }















