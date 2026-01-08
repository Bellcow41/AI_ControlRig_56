"""
증분 학습 스크립트 - 승인된 매핑 데이터로 LoRA 추가 학습
"""
import os
import json
import torch
from datetime import datetime
from transformers import AutoModelForCausalLM, AutoTokenizer, TrainingArguments, Trainer, DataCollatorForLanguageModeling
from peft import PeftModel, LoraConfig, get_peft_model
from datasets import Dataset

# 경로 설정
BASE_DIR = os.path.dirname(os.path.dirname(__file__))
APPROVED_DATA_PATH = os.path.join(BASE_DIR, "02_data_prep", "approved_mappings.jsonl")
CHECKPOINT_DIR = os.path.join(BASE_DIR, "03_fine_tuning", "checkpoints", "bone_mapping_lora")
NEW_CHECKPOINT_DIR = os.path.join(BASE_DIR, "03_fine_tuning", "checkpoints", "bone_mapping_lora_updated")

# 모델 설정
BASE_MODEL = "Qwen/Qwen2.5-Coder-7B-Instruct"

def load_approved_data():
    """승인된 학습 데이터 로드"""
    if not os.path.exists(APPROVED_DATA_PATH):
        print(f"[Train] No approved data found at {APPROVED_DATA_PATH}")
        return []
    
    data = []
    with open(APPROVED_DATA_PATH, 'r', encoding='utf-8') as f:
        for line in f:
            try:
                entry = json.loads(line.strip())
                data.append(entry)
            except:
                continue
    
    print(f"[Train] Loaded {len(data)} approved samples")
    return data

def format_for_training(data):
    """학습 데이터 형식 변환"""
    formatted = []
    for entry in data:
        text = f"### Input:\n{entry['input']}\n\n### Output:\n{entry['output']}"
        formatted.append({"text": text})
    return formatted

def train():
    print("="*60)
    print(f"[Incremental Training] {datetime.now()}")
    print("="*60)
    
    # 데이터 로드
    data = load_approved_data()
    if len(data) < 5:
        print("[Train] Not enough data for training (minimum 5 samples)")
        return False
    
    formatted_data = format_for_training(data)
    dataset = Dataset.from_list(formatted_data)
    
    print(f"[Train] Dataset size: {len(dataset)}")
    
    # 토크나이저 로드
    print("[Train] Loading tokenizer...")
    tokenizer = AutoTokenizer.from_pretrained(BASE_MODEL, trust_remote_code=True)
    tokenizer.pad_token = tokenizer.eos_token
    
    # 기존 LoRA 모델 로드
    print("[Train] Loading base model with existing LoRA...")
    base_model = AutoModelForCausalLM.from_pretrained(
        BASE_MODEL,
        torch_dtype=torch.float16,
        device_map="auto",
        trust_remote_code=True
    )
    
    if os.path.exists(CHECKPOINT_DIR):
        print(f"[Train] Loading existing LoRA from {CHECKPOINT_DIR}")
        model = PeftModel.from_pretrained(base_model, CHECKPOINT_DIR)
        # LoRA를 학습 가능하게 설정
        model = model.merge_and_unload()
        
        # 새 LoRA 적용
        lora_config = LoraConfig(
            r=16,
            lora_alpha=32,
            target_modules=["q_proj", "v_proj", "k_proj", "o_proj"],
            lora_dropout=0.05,
            bias="none",
            task_type="CAUSAL_LM"
        )
        model = get_peft_model(model, lora_config)
    else:
        print("[Train] No existing LoRA, creating new one")
        lora_config = LoraConfig(
            r=16,
            lora_alpha=32,
            target_modules=["q_proj", "v_proj", "k_proj", "o_proj"],
            lora_dropout=0.05,
            bias="none",
            task_type="CAUSAL_LM"
        )
        model = get_peft_model(base_model, lora_config)
    
    model.print_trainable_parameters()
    
    # 데이터 토크나이즈
    def tokenize_function(examples):
        return tokenizer(examples["text"], truncation=True, max_length=512, padding="max_length")
    
    tokenized_dataset = dataset.map(tokenize_function, batched=True, remove_columns=["text"])
    tokenized_dataset = tokenized_dataset.add_column("labels", tokenized_dataset["input_ids"])
    
    # 학습 설정
    training_args = TrainingArguments(
        output_dir=NEW_CHECKPOINT_DIR,
        num_train_epochs=3,
        per_device_train_batch_size=1,
        gradient_accumulation_steps=4,
        learning_rate=1e-4,
        fp16=True,
        logging_steps=10,
        save_strategy="epoch",
        warmup_ratio=0.1,
        optim="adamw_torch",
    )
    
    # 데이터 콜레이터
    data_collator = DataCollatorForLanguageModeling(tokenizer=tokenizer, mlm=False)
    
    # 트레이너 설정
    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=tokenized_dataset,
        data_collator=data_collator,
    )
    
    # 학습 실행
    print("[Train] Starting training...")
    trainer.train()
    
    # 모델 저장
    print(f"[Train] Saving to {NEW_CHECKPOINT_DIR}")
    trainer.save_model(NEW_CHECKPOINT_DIR)
    tokenizer.save_pretrained(NEW_CHECKPOINT_DIR)
    
    # 기존 체크포인트 백업 및 교체
    import shutil
    backup_dir = CHECKPOINT_DIR + f"_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    if os.path.exists(CHECKPOINT_DIR):
        shutil.move(CHECKPOINT_DIR, backup_dir)
        print(f"[Train] Backed up old model to {backup_dir}")
    
    shutil.move(NEW_CHECKPOINT_DIR, CHECKPOINT_DIR)
    print(f"[Train] Updated model at {CHECKPOINT_DIR}")
    
    # 학습 완료 후 승인 데이터 아카이브
    archive_path = APPROVED_DATA_PATH.replace('.jsonl', f'_trained_{datetime.now().strftime("%Y%m%d_%H%M%S")}.jsonl')
    shutil.move(APPROVED_DATA_PATH, archive_path)
    print(f"[Train] Archived training data to {archive_path}")
    
    print("="*60)
    print("[Train] Incremental training completed!")
    print("="*60)
    
    return True

if __name__ == "__main__":
    train()







