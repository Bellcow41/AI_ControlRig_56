print("=" * 50)
print("Environment Check")
print("=" * 50)

# PyTorch
import torch
print(f"[OK] PyTorch: {torch.__version__}")
print(f"[OK] CUDA: {torch.cuda.is_available()}")
print(f"[OK] GPU: {torch.cuda.get_device_name(0)}")
print(f"[OK] VRAM: {torch.cuda.get_device_properties(0).total_memory / 1e9:.1f} GB")

# Transformers
import transformers
print(f"[OK] Transformers: {transformers.__version__}")

# Datasets
import datasets
print(f"[OK] Datasets: {datasets.__version__}")

# Accelerate
import accelerate
print(f"[OK] Accelerate: {accelerate.__version__}")

# PEFT
import peft
print(f"[OK] PEFT: {peft.__version__}")

# TRL
import trl
print(f"[OK] TRL: {trl.__version__}")

# Unsloth
try:
    from unsloth import FastLanguageModel
    print(f"[OK] Unsloth: Installed")
except Exception as e:
    print(f"[WARN] Unsloth: {e}")

print("=" * 50)
print("All packages ready for fine-tuning!")
print("=" * 50)
