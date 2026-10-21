#!/bin/bash
# vexp-hint: event-driven orientation hint (UserPromptSubmit). Fails open.
# Uses the newest installed vexp VS Code extension binary; exits quietly if
# vexp is not installed on this machine.
VEXP_BIN="$(ls -d "$HOME"/.vscode/extensions/vexp.vexp-vscode-*/binaries/*/vexp-core* 2>/dev/null | sort -V | tail -n 1)"
[ -n "$VEXP_BIN" ] && [ -x "$VEXP_BIN" ] || exit 0
VEXP_HOOK_AGENT="claude-code" "$VEXP_BIN" prompt-hint 2>/dev/null
exit 0
