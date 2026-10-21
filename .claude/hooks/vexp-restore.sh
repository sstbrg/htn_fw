#!/bin/bash
# vexp-restore: context lifecycle restore on SessionStart (compact/resume). Fails open.
# Uses the newest installed vexp VS Code extension binary; exits quietly if
# vexp is not installed on this machine.
VEXP_BIN="$(ls -d "$HOME"/.vscode/extensions/vexp.vexp-vscode-*/binaries/*/vexp-core* 2>/dev/null | sort -V | tail -n 1)"
[ -n "$VEXP_BIN" ] && [ -x "$VEXP_BIN" ] || exit 0
"$VEXP_BIN" session-context 2>/dev/null
exit 0
