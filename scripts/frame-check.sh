#!/usr/bin/env bash
# frame-check.sh — is the Frame reachable, and what's on it?
# Usage: ./scripts/frame-check.sh [ssh-host]     (default host: frame)

set -uo pipefail
HOST="${1:-frame}"

say() { printf '\n=== %s ===\n' "$1"; }

say "Reachability: $HOST"
if ! ssh -o BatchMode=yes -o ConnectTimeout=6 "$HOST" 'true' 2>/dev/null; then
  echo "OFFLINE — could not SSH to '$HOST'."
  echo "Bring the headset up, confirm Developer Mode, then re-run."
  exit 1
fi
echo "ONLINE"

say "System"
ssh "$HOST" 'uname -m; . /etc/os-release 2>/dev/null; echo "OS: $PRETTY_NAME"; uptime'

say "SteamVR processes"
ssh "$HOST" 'ps -eo pid,comm,args | grep -iE "steamvr|vrserver|vrmonitor|v4l2cam" | grep -v grep | head -20'

say "Headset-view device"
ssh "$HOST" 'ls -l /dev/video99 2>&1; v4l2-ctl -d /dev/video99 --all 2>/dev/null | head -20'

say "SteamVR binaries"
ssh "$HOST" 'ls /opt/steamvr/bin/linuxarm64/ 2>/dev/null | head -40'

say "Recently modified tracking-ish files (last 2 days)"
ssh "$HOST" 'find /home/steamos /tmp -maxdepth 4 -newermt "-2 days" -type f 2>/dev/null \
  | grep -iE "track|dataset|capture|vrserver" | head -30'

say "Done"
