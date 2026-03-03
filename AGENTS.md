# AGENTS.md

## Workspace Scope
- Working root is `c:\Users\stara\Desktop\DadeumStudio`.
- Read/write operations are allowed only inside this directory.
- Do not access paths outside this workspace unless the user explicitly requests it.

## Runtime Defaults
- Use Codex with `--sandbox workspace-write` and `--ask-for-approval untrusted`.
- Recommended launch command: `codex -C c:\Users\stara\Desktop\DadeumStudio -s workspace-write -a untrusted`.
- This keeps write/read actions scoped to this workspace and auto-approves trusted read-only commands.

## Auto Commit Policy
- After each completed code-editing task, create a commit automatically.
- Use this sequence:
  1. `git add -A`
  2. `git commit -m "<type(scope): summary>"`
- Commit message must be concise and describe the exact change.
- If there are no staged changes, skip commit and report `no changes to commit`.
- If commit fails, report the error immediately and do not continue with more edits.

## Safety Rules
- Never run destructive git operations (e.g., `reset --hard`, force checkout, history rewrite) unless the user explicitly asks.
- Prefer one logical commit per user request.