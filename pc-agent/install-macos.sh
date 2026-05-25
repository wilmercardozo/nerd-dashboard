#!/usr/bin/env bash
# Registra el agente Nerd Dashboard como LaunchAgent (arranca al iniciar sesion).
# Uso:  bash install-macos.sh
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="$(command -v python3)"

"$PY" -m pip install -r "$DIR/requirements.txt"

PLIST="$HOME/Library/LaunchAgents/com.nerddashboard.agent.plist"
mkdir -p "$HOME/Library/LaunchAgents"
cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>com.nerddashboard.agent</string>
  <key>ProgramArguments</key>
  <array><string>$PY</string><string>$DIR/agent.py</string></array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardErrorPath</key><string>/tmp/nerddashboard-agent.log</string>
  <key>StandardOutPath</key><string>/tmp/nerddashboard-agent.log</string>
</dict>
</plist>
EOF

launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo "Agente cargado (com.nerddashboard.agent), puerto 8765. Log: /tmp/nerddashboard-agent.log"
echo "Quitar:  launchctl unload \"$PLIST\" && rm \"$PLIST\""
