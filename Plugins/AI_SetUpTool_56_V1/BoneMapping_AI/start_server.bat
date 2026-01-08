@echo off
cd /d "%~dp0"
python\python.exe 04_inference\api_server.py > server_log.txt 2>&1

