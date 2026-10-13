# env.ps1 - Activates ESP-IDF v6.0.2 environment in current PowerShell session
$activationScript = "C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1"
if (Test-Path $activationScript) {
    Write-Host "[ESP-IDF] Activating environment..." -ForegroundColor Cyan
    . $activationScript
    Write-Host "[ESP-IDF] Environment ready! You can now run idf.py commands." -ForegroundColor Green
} else {
    Write-Error "[ESP-IDF] Activation script not found at: $activationScript"
}
