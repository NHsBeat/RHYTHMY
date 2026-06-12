# RHYTHMY — WSL Setup Script
# Run this once as Administrator, then reboot.
# After reboot Claude will finish the rest automatically.

Write-Host "Enabling WSL..." -ForegroundColor Cyan

dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart | Out-Null
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart | Out-Null

Write-Host "Installing Ubuntu..." -ForegroundColor Cyan
wsl --install -d Ubuntu --no-launch

Write-Host ""
Write-Host "Done! Now REBOOT your PC." -ForegroundColor Green
Write-Host "After reboot, return to Claude and type: готово" -ForegroundColor Yellow
Write-Host ""
pause
