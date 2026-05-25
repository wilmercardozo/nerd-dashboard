# Registra el agente Nerd Dashboard como tarea programada al iniciar sesion.
# Uso:  powershell -ExecutionPolicy Bypass -File install-windows.ps1
$ErrorActionPreference = "Stop"
$dir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$agent = Join-Path $dir "agent.py"

# Instalar dependencias
python -m pip install -r (Join-Path $dir "requirements.txt")

# pythonw = sin ventana de consola
$py = (Get-Command pythonw -ErrorAction SilentlyContinue).Source
if (-not $py) { $py = (Get-Command python).Source }

$action   = New-ScheduledTaskAction -Execute $py -Argument "`"$agent`""
$trigger  = New-ScheduledTaskTrigger -AtLogOn
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Register-ScheduledTask -TaskName "NerdDashboardAgent" -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName "NerdDashboardAgent"

Write-Host "Agente registrado e iniciado (tarea 'NerdDashboardAgent'), puerto 8765."
Write-Host "Quitar:  Unregister-ScheduledTask -TaskName NerdDashboardAgent -Confirm:`$false"
