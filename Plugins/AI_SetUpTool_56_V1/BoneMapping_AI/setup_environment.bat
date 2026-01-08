@echo off
echo ========================================
echo   Bone Mapping AI - Environment Setup
echo ========================================
echo.

:: 1. Python 버전 확인
echo [1/6] Checking Python version...
python --version
if errorlevel 1 (
    echo ERROR: Python not found! Please install Python 3.10+
    pause
    exit /b 1
)

:: 2. 가상환경 생성
echo.
echo [2/6] Creating virtual environment...
if not exist "venv" (
    python -m venv venv
    echo Virtual environment created.
) else (
    echo Virtual environment already exists.
)

:: 3. 가상환경 활성화
echo.
echo [3/6] Activating virtual environment...
call venv\Scripts\activate.bat

:: 4. pip 업그레이드
echo.
echo [4/6] Upgrading pip...
python -m pip install --upgrade pip

:: 5. PyTorch 설치 (CUDA 12.1)
echo.
echo [5/6] Installing PyTorch with CUDA support...
echo This may take a while...
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121

:: 6. 기타 패키지 설치
echo.
echo [6/6] Installing other dependencies...
pip install -r requirements.txt

:: Unsloth 설치 (별도)
echo.
echo Installing Unsloth (fast fine-tuning library)...
pip install "unsloth[colab-new] @ git+https://github.com/unslothai/unsloth.git"

echo.
echo ========================================
echo   Setup Complete!
echo ========================================
echo.
echo Next steps:
echo   1. Prepare your training data in examples/sample_mappings/
echo   2. Run: python 02_data_processing/create_dataset.py
echo   3. Run: python 03_fine_tuning/train.py
echo.
pause















