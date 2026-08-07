#!/bin/bash
# install_service.sh
# Run once on the Raspberry Pi to install and enable the KeyBox autostart service.
# Usage: bash scripts/install_service.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_SRC="$SCRIPT_DIR/keybox.service"
SERVICE_DEST="/etc/systemd/system/keybox.service"

echo "Installing KeyBox systemd service..."
sudo cp "$SERVICE_SRC" "$SERVICE_DEST"
sudo systemctl daemon-reload
sudo systemctl enable keybox.service
sudo systemctl restart keybox.service

echo "Done. KeyBox will now start automatically on boot."
echo "Check status with: sudo systemctl status keybox.service"
