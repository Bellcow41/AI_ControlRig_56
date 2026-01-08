@echo off
chcp 65001 > nul
echo ============================================
echo   Control Rig Tool - Python Environment Setup
echo ============================================
echo.

REM Check Python installation
python --version > nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found!
    echo Please install Python 3.10 or higher from:
    echo https://www.python.org/downloads/
    echo.
    echo Make sure to check "Add Python to PATH" during installation.
    pause
    exit /b 1
)

echo [OK] Python found
python --version
echo.

REM Navigate to BoneMapping_AI folder
cd /d "%~dp0BoneMapping_AI"

echo Installing required packages...
echo.

REM Install PyTorch with CUDA support
echo [1/4] Installing PyTorch (CUDA 12.1)...
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121

REM Install Transformers and related packages
echo [2/4] Installing Transformers...
pip install transformers>=4.40.0

REM Install PEFT for LoRA
echo [3/4] Installing PEFT...
pip install peft>=0.10.0

REM Install FastAPI and other dependencies
echo [4/4] Installing FastAPI and utilities...
pip install fastapi uvicorn httpx accelerate datasets

echo.
echo ============================================
echo   Setup Complete!
echo ============================================
echo.
echo You can now use the Control Rig Tool plugin.
echo The AI server will start automatically when you open the Unreal Editor.
echo.
pause










