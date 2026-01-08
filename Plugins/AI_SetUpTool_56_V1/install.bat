@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║         Control Rig Tool - One-Click Installer               ║
echo ║                                                               ║
echo ║  This will install all required dependencies automatically.  ║
echo ║  Internet connection required for package download.          ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

REM Set paths
set "PLUGIN_DIR=%~dp0"
set "PYTHON_DIR=%PLUGIN_DIR%BoneMapping_AI\python"
set "PYTHON_EXE=%PYTHON_DIR%\python.exe"
set "PIP_EXE=%PYTHON_DIR%\Scripts\pip.exe"

REM Check if embedded Python exists
if not exist "%PYTHON_EXE%" (
    echo [ERROR] Embedded Python not found!
    echo Expected location: %PYTHON_EXE%
    pause
    exit /b 1
)

echo [1/5] Embedded Python found: %PYTHON_EXE%
"%PYTHON_EXE%" --version
echo.

REM Create Lib\site-packages folder
if not exist "%PYTHON_DIR%\Lib\site-packages" (
    mkdir "%PYTHON_DIR%\Lib\site-packages"
)

REM Install pip if not already installed
if not exist "%PIP_EXE%" (
    echo [2/5] Installing pip...
    "%PYTHON_EXE%" "%PYTHON_DIR%\get-pip.py" --no-warn-script-location
    if errorlevel 1 (
        echo [ERROR] Failed to install pip!
        pause
        exit /b 1
    )
) else (
    echo [2/5] pip already installed.
)
echo.

REM Install PyTorch with CUDA support
echo [3/5] Installing PyTorch with CUDA 12.1 support...
echo      This may take several minutes depending on your internet speed...
"%PYTHON_EXE%" -m pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121 --no-warn-script-location -q
if errorlevel 1 (
    echo [WARNING] CUDA version failed. Trying CPU version...
    "%PYTHON_EXE%" -m pip install torch torchvision torchaudio --no-warn-script-location -q
)
echo      PyTorch installed.
echo.

REM Install Transformers and PEFT
echo [4/5] Installing Transformers and PEFT (LoRA)...
"%PYTHON_EXE%" -m pip install transformers>=4.40.0 peft>=0.10.0 accelerate datasets --no-warn-script-location -q
echo      Transformers installed.
echo.

REM Install FastAPI and utilities
echo [5/5] Installing FastAPI and utilities...
"%PYTHON_EXE%" -m pip install fastapi uvicorn httpx numpy --no-warn-script-location -q
echo      FastAPI installed.
echo.

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                    Installation Complete!                     ║
echo ╠══════════════════════════════════════════════════════════════╣
echo ║  You can now open Unreal Engine and use Control Rig Tool.    ║
echo ║                                                               ║
echo ║  The AI Bone Mapping server will start automatically         ║
echo ║  when you open the Unreal Editor.                            ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

pause

