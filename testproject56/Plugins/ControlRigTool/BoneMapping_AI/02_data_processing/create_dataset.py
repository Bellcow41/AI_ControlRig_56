"""
본 매핑 데이터를 LLM 파인튜닝용 데이터셋으로 변환하는 스크립트

학습 데이터 형식: Alpaca (instruction, input, output)
"""

import json
import os
import random
from pathlib import Path
from typing import List, Dict, Any


# ============================================================
# 프롬프트 템플릿
# ============================================================

SYSTEM_PROMPT = """You are a bone mapping expert for skeletal meshes in game engines. 
Your task is to map bones from a source skeleton to a target skeleton based on their names and anatomical positions.
Always provide accurate mappings considering naming conventions from different DCC tools (Maya, 3ds Max, Blender, Mixamo, etc.)."""

INSTRUCTION_TEMPLATES = [
    "Map the bones from the source skeleton to the UE5 Mannequin skeleton.",
    "Create a bone mapping from the given skeleton to Unreal Engine 5 standard skeleton.",
    "Provide bone mapping for retargeting from the source to target skeleton.",
    "Match each source bone to its corresponding target bone.",
    "Generate a bone mapping table for the following skeleton.",
]

INPUT_TEMPLATES = [
    "Source skeleton type: {source_type}\nSource bones:\n{source_bones}\n\nTarget skeleton type: {target_type}\nTarget bones:\n{target_bones}",
    "I have a {source_type} skeleton with the following bones:\n{source_bones}\n\nMap these to {target_type}:\n{target_bones}",
    "Source ({source_type}):\n{source_bones}\n\nTarget ({target_type}):\n{target_bones}",
]

OUTPUT_TEMPLATE = """Here is the bone mapping from {source_type} to {target_type}:

{mapping_text}"""


# ============================================================
# 데이터 변환 함수
# ============================================================

def format_bones_list(bones: List[str], numbered: bool = True) -> str:
    """본 리스트를 포맷팅된 문자열로 변환"""
    if numbered:
        return "\n".join([f"{i+1}. {bone}" for i, bone in enumerate(bones)])
    else:
        return ", ".join(bones)


def format_mapping(mapping: Dict[str, str]) -> str:
    """매핑 딕셔너리를 포맷팅된 문자열로 변환"""
    lines = []
    for source, target in mapping.items():
        lines.append(f"  {source} -> {target}")
    return "\n".join(lines)


def create_training_example(
    source_type: str,
    target_type: str,
    source_bones: List[str],
    target_bones: List[str],
    mapping: Dict[str, str]
) -> Dict[str, str]:
    """단일 학습 데이터 생성"""
    
    # 랜덤 템플릿 선택
    instruction = random.choice(INSTRUCTION_TEMPLATES)
    input_template = random.choice(INPUT_TEMPLATES)
    
    # 입력 생성
    input_text = input_template.format(
        source_type=source_type,
        target_type=target_type,
        source_bones=format_bones_list(source_bones),
        target_bones=format_bones_list(target_bones)
    )
    
    # 출력 생성
    output_text = OUTPUT_TEMPLATE.format(
        source_type=source_type,
        target_type=target_type,
        mapping_text=format_mapping(mapping)
    )
    
    return {
        "instruction": instruction,
        "input": input_text,
        "output": output_text,
        "system": SYSTEM_PROMPT
    }


def create_partial_mapping_example(
    source_type: str,
    target_type: str,
    source_bones: List[str],
    target_bones: List[str],
    mapping: Dict[str, str],
    num_bones: int = 5
) -> Dict[str, str]:
    """부분 본 매핑 학습 데이터 (일부 본만 매핑)"""
    
    # 랜덤으로 일부 본 선택
    selected_sources = random.sample(source_bones, min(num_bones, len(source_bones)))
    partial_mapping = {k: v for k, v in mapping.items() if k in selected_sources}
    
    instruction = f"Map only these {len(selected_sources)} bones from the source to target skeleton."
    
    input_text = f"""Source skeleton: {source_type}
Bones to map: {', '.join(selected_sources)}

Available target bones ({target_type}):
{format_bones_list(target_bones)}"""
    
    output_text = f"""Bone mapping results:

{format_mapping(partial_mapping)}"""
    
    return {
        "instruction": instruction,
        "input": input_text,
        "output": output_text,
        "system": SYSTEM_PROMPT
    }


def create_reverse_mapping_example(
    source_type: str,
    target_type: str,
    source_bones: List[str],
    target_bones: List[str],
    mapping: Dict[str, str]
) -> Dict[str, str]:
    """역방향 매핑 (타겟 -> 소스)"""
    
    reverse_mapping = {v: k for k, v in mapping.items()}
    
    return create_training_example(
        source_type=target_type,
        target_type=source_type,
        source_bones=target_bones,
        target_bones=source_bones,
        mapping=reverse_mapping
    )


def create_question_answer_examples(mapping_data: Dict) -> List[Dict[str, str]]:
    """Q&A 형식의 학습 데이터 생성"""
    
    examples = []
    mapping = mapping_data["mapping"]
    source_type = mapping_data["source_type"]
    target_type = mapping_data["target_type"]
    
    # 개별 본 매핑 질문
    for source, target in mapping.items():
        examples.append({
            "instruction": f"What is the target bone for '{source}' in {target_type}?",
            "input": f"Source skeleton: {source_type}\nTarget skeleton: {target_type}",
            "output": f"The bone '{source}' from {source_type} maps to '{target}' in {target_type}.",
            "system": SYSTEM_PROMPT
        })
    
    # 신체 부위별 질문
    body_parts = {
        "spine": ["spine", "Spine", "pelvis", "Hips"],
        "arm": ["arm", "Arm", "hand", "Hand", "shoulder", "Shoulder"],
        "leg": ["leg", "Leg", "thigh", "Thigh", "foot", "Foot"],
        "head": ["head", "Head", "neck", "Neck"]
    }
    
    for part_name, keywords in body_parts.items():
        part_bones = [b for b in mapping.keys() if any(kw in b for kw in keywords)]
        if part_bones:
            part_mapping = {k: v for k, v in mapping.items() if k in part_bones}
            examples.append({
                "instruction": f"Map the {part_name} bones from {source_type} to {target_type}.",
                "input": f"Source {part_name} bones: {', '.join(part_bones)}",
                "output": f"{part_name.capitalize()} bone mapping:\n{format_mapping(part_mapping)}",
                "system": SYSTEM_PROMPT
            })
    
    return examples


# ============================================================
# 데이터 증강
# ============================================================

def augment_bone_names(bones: List[str], mapping: Dict[str, str]) -> tuple:
    """본 이름 변형으로 데이터 증강"""
    
    variations = []
    
    # 변형 규칙
    transformations = [
        # 대소문자 변형
        lambda x: x.lower(),
        lambda x: x.upper(),
        lambda x: x.title(),
        # 구분자 변형
        lambda x: x.replace("_", "-"),
        lambda x: x.replace("_", " "),
        lambda x: x.replace(":", "_"),
        # 접두사 추가/제거
        lambda x: f"bone_{x}" if not x.startswith("bone") else x,
        lambda x: x.replace("mixamorig:", ""),
        lambda x: x.replace("Bip001_", ""),
    ]
    
    for transform in transformations:
        new_bones = [transform(b) for b in bones]
        new_mapping = {transform(k): v for k, v in mapping.items()}
        variations.append((new_bones, new_mapping))
    
    return variations


# ============================================================
# 메인 데이터셋 생성
# ============================================================

def load_raw_mappings(mappings_file: str) -> List[Dict]:
    """원본 매핑 파일 로드"""
    with open(mappings_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return data.get("mappings", [])


def generate_dataset(
    mappings_file: str,
    output_file: str,
    augment: bool = True,
    include_qa: bool = True,
    include_partial: bool = True,
    include_reverse: bool = True
):
    """전체 학습 데이터셋 생성"""
    
    raw_mappings = load_raw_mappings(mappings_file)
    dataset = []
    
    for mapping_data in raw_mappings:
        source_type = mapping_data["source_type"]
        target_type = mapping_data["target_type"]
        source_bones = mapping_data["source_bones"]
        target_bones = mapping_data["target_bones"]
        mapping = mapping_data["mapping"]
        
        # 1. 기본 전체 매핑
        dataset.append(create_training_example(
            source_type, target_type, source_bones, target_bones, mapping
        ))
        
        # 2. 역방향 매핑
        if include_reverse:
            dataset.append(create_reverse_mapping_example(
                source_type, target_type, source_bones, target_bones, mapping
            ))
        
        # 3. 부분 매핑 (여러 크기)
        if include_partial:
            for num_bones in [3, 5, 10]:
                if len(source_bones) >= num_bones:
                    dataset.append(create_partial_mapping_example(
                        source_type, target_type, source_bones, target_bones, mapping, num_bones
                    ))
        
        # 4. Q&A 형식
        if include_qa:
            qa_examples = create_question_answer_examples(mapping_data)
            dataset.extend(qa_examples)
        
        # 5. 데이터 증강
        if augment:
            variations = augment_bone_names(source_bones, mapping)
            for aug_bones, aug_mapping in variations[:3]:  # 상위 3개만
                dataset.append(create_training_example(
                    f"{source_type}_variant", target_type,
                    aug_bones, target_bones, aug_mapping
                ))
    
    # 셔플
    random.shuffle(dataset)
    
    # 저장
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(dataset, f, indent=2, ensure_ascii=False)
    
    print(f"Generated {len(dataset)} training examples")
    print(f"Saved to: {output_file}")
    
    return dataset


def split_dataset(
    dataset_file: str,
    train_ratio: float = 0.9,
    output_dir: str = None
):
    """데이터셋을 train/validation으로 분할"""
    
    with open(dataset_file, 'r', encoding='utf-8') as f:
        dataset = json.load(f)
    
    random.shuffle(dataset)
    split_idx = int(len(dataset) * train_ratio)
    
    train_data = dataset[:split_idx]
    val_data = dataset[split_idx:]
    
    if output_dir is None:
        output_dir = os.path.dirname(dataset_file)
    
    train_file = os.path.join(output_dir, "train.json")
    val_file = os.path.join(output_dir, "validation.json")
    
    with open(train_file, 'w', encoding='utf-8') as f:
        json.dump(train_data, f, indent=2, ensure_ascii=False)
    
    with open(val_file, 'w', encoding='utf-8') as f:
        json.dump(val_data, f, indent=2, ensure_ascii=False)
    
    print(f"Train: {len(train_data)} examples -> {train_file}")
    print(f"Validation: {len(val_data)} examples -> {val_file}")


# ============================================================
# CLI
# ============================================================

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate bone mapping training dataset")
    parser.add_argument("--input", "-i", required=True, help="Input mappings JSON file")
    parser.add_argument("--output", "-o", required=True, help="Output dataset JSON file")
    parser.add_argument("--no-augment", action="store_true", help="Disable data augmentation")
    parser.add_argument("--no-qa", action="store_true", help="Disable Q&A examples")
    parser.add_argument("--split", action="store_true", help="Split into train/val")
    parser.add_argument("--train-ratio", type=float, default=0.9, help="Train split ratio")
    
    args = parser.parse_args()
    
    # 데이터셋 생성
    generate_dataset(
        mappings_file=args.input,
        output_file=args.output,
        augment=not args.no_augment,
        include_qa=not args.no_qa
    )
    
    # 분할
    if args.split:
        split_dataset(args.output, args.train_ratio)















