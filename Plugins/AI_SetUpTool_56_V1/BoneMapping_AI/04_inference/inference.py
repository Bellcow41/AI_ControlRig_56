"""
Bone Mapping AI Inference Module
AI-centered approach: AI uses context (parent, children, anchors, chain) to make decisions
The AI was trained with hierarchical knowledge from Maya skeleton structure
"""
import os
import re
import torch
from typing import Dict, List, Optional, Tuple
from transformers import AutoModelForCausalLM, AutoTokenizer
from peft import PeftModel

# Model paths
BASE_MODEL = "Qwen/Qwen2.5-Coder-7B-Instruct"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LORA_PATH = os.path.join(SCRIPT_DIR, "..", "03_fine_tuning", "checkpoints", "bone_mapping_lora")

# Valid UE5 mannequin bones
VALID_UE5_BONES = [
    'root', 'pelvis', 
    'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05',
    'neck_01', 'neck_02', 'head',
    'clavicle_l', 'clavicle_r', 
    'upperarm_l', 'upperarm_r',
    'lowerarm_l', 'lowerarm_r', 
    'hand_l', 'hand_r',
    'thigh_l', 'thigh_r', 
    'calf_l', 'calf_r',
    'foot_l', 'foot_r', 
    'ball_l', 'ball_r',
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
]


class BoneMappingInference:
    """AI-based bone mapping using fine-tuned LoRA model with hierarchical knowledge"""
    
    def __init__(self, use_gpu: bool = True):
        self.model = None
        self.tokenizer = None
        self.device = "cuda" if use_gpu and torch.cuda.is_available() else "cpu"
        self.loaded = False
        
    def load_model(self) -> bool:
        """Load the fine-tuned LoRA model"""
        try:
            print(f"[AI] Loading base model: {BASE_MODEL}")
            print(f"[AI] LoRA path: {LORA_PATH}")
            print(f"[AI] Device: {self.device}")
            
            # Load tokenizer
            self.tokenizer = AutoTokenizer.from_pretrained(BASE_MODEL, trust_remote_code=True)
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            # Load base model
            self.model = AutoModelForCausalLM.from_pretrained(
                BASE_MODEL,
                torch_dtype=torch.float16 if self.device == "cuda" else torch.float32,
                device_map="auto" if self.device == "cuda" else None,
                trust_remote_code=True
            )
            
            # Load LoRA adapter
            adapter_path = os.path.join(LORA_PATH, "adapter_config.json")
            if os.path.exists(adapter_path):
                print(f"[AI] Loading LoRA adapter...")
                self.model = PeftModel.from_pretrained(self.model, LORA_PATH)
                print(f"[AI] LoRA adapter loaded successfully")
            else:
                print(f"[AI] Warning: LoRA adapter not found at {LORA_PATH}")
                
            if self.device == "cpu":
                self.model = self.model.to(self.device)
                
            self.model.eval()
            self.loaded = True
            print(f"[AI] Model loaded successfully!")
            return True
            
        except Exception as e:
            print(f"[AI] Error loading model: {e}")
            import traceback
            traceback.print_exc()
            self.loaded = False
            return False
    
    def predict_single(self, bone_name: str, parent: str = None, children: List[str] = None,
                       anchors: List[str] = None, chain_type: str = None) -> str:
        """
        Predict mapping for a single bone with full context
        
        The AI was trained with hierarchical knowledge:
        - Parent-child relationships
        - Anchor points (known bones in the chain)
        - Chain types (left_arm, right_leg, spine, etc.)
        
        Args:
            bone_name: Source bone name
            parent: Parent bone name
            children: List of child bone names
            anchors: Known anchor bones in this chain
            chain_type: Type of chain (left_arm, right_arm, left_leg, right_leg, spine, etc.)
            
        Returns:
            Target bone name or "[SKIP]" for auxiliary bones
        """
        if not self.loaded:
            return None
        
        # Build context-aware prompt (same format as training data)
        if anchors and chain_type:
            # Full context with anchors
            prompt = f"Bone: {bone_name}, Parent: {parent or 'None'}, Children: {children or []}, Anchors: {anchors}, ChainType: {chain_type}"
            instruction = "Map this bone to UE5 mannequin standard. Consider bone name, parent, children, anchors, and chain type."
        elif parent or children:
            # Basic context
            children_str = str(children) if children else "[]"
            parent_str = parent if parent else "None"
            prompt = f"Bone: {bone_name}, Parent: {parent_str}, Children: {children_str}"
            instruction = "Map this bone to UE5 mannequin standard. Consider bone name, parent, and children."
        else:
            # Name only
            prompt = bone_name
            instruction = "Map this bone name to UE5 mannequin standard"
        
        messages = [
            {"role": "system", "content": "You are a bone mapping expert. Map source bones to UE5 mannequin standard bones. Output only the target bone name or [SKIP] for auxiliary bones."},
            {"role": "user", "content": f"{instruction}\n{prompt}"}
        ]
        
        try:
            text = self.tokenizer.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
            inputs = self.tokenizer(text, return_tensors="pt").to(self.device)
            
            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=30,
                    do_sample=False,
                    pad_token_id=self.tokenizer.pad_token_id
                )
            
            response = self.tokenizer.decode(outputs[0][inputs['input_ids'].shape[1]:], skip_special_tokens=True)
            return self._parse_response(response.strip())
            
        except Exception as e:
            print(f"[AI] Prediction error for {bone_name}: {e}")
            return None
    
    def _parse_response(self, response: str) -> str:
        """Parse AI response to extract bone name"""
        if not response:
            return None
            
        response = response.strip()
        
        # Check for SKIP
        if "[SKIP]" in response.upper() or response.upper() == "SKIP":
            return "[SKIP]"
        
        # Clean up
        response = response.replace('"', '').replace("'", "").strip()
        
        # Take first line only
        lines = response.split('\n')
        if lines:
            response = lines[0].strip()
        
        # Remove common prefixes
        for prefix in ['target:', 'output:', 'result:', 'mapping:', '->', '=>']:
            if response.lower().startswith(prefix):
                response = response[len(prefix):].strip()
        
        # Remove arrow notation if present
        if '->' in response:
            response = response.split('->')[-1].strip()
        
        response = response.lower().strip()
        
        # Validate against known bones
        if response in VALID_UE5_BONES:
            return response
        
        # Partial match
        for bone in VALID_UE5_BONES:
            if bone in response or response in bone:
                return bone
        
        # If it looks reasonable, return it
        if len(response) < 30 and response.replace('_', '').isalnum():
            return response
                
        return None


# Singleton instance
_inference_instance = None

def get_inference() -> BoneMappingInference:
    """Get or create singleton inference instance"""
    global _inference_instance
    if _inference_instance is None:
        _inference_instance = BoneMappingInference()
    return _inference_instance
