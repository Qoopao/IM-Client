@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\deploy-client.ps1" %*
set "deploy_exit_code=%errorlevel%"
echo.
if not "%deploy_exit_code%"=="0" echo Deployment failed. See the error above.
if "%~1"=="" pause
exit /b %deploy_exit_code%
