#!/usr/bin/env bash
# TDR Edge-AI Dashboard Launcher for Raspberry Pi / Linux
cd "$(dirname "$0")"

echo "=================================================="
echo "  🚀 Starting TDR Edge-AI Diagnostic Dashboard"
echo "=================================================="

# Ensure pyserial is installed
python3 -c "import serial" 2>/dev/null || {
    echo "[*] Installing pyserial..."
    python3 -m pip install pyserial
}

# Run the web dashboard server
python3 app.py
