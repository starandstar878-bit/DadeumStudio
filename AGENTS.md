AGENTS.md
1. Workspace Scope and Permissions
Root Directory: c:\Users\stara\Desktop\DadeumStudio

Access Control: All read and write operations must be confined to the root directory. Accessing external paths is strictly prohibited unless a specific request is made by the user.

Truthfulness: If the source or basis of an answer is unclear, you must explicitly ask for the relevant files or state "I do not know" (모르겠습니다) to avoid hallucinations.

2. Runtime Configuration
Model Profile:

Model: gpt-5.3-codex

Reasoning Effort: xhigh

Personality: pragmatic

Execution Policy:

Sandbox Mode: workspace-write

Approval Policy: never

Primary Launch Command: codex -C c:\Users\stara\Desktop\DadeumStudio -s workspace-write -a never

3. Coding Standards and Documentation
Minimal Commenting: Do not include redundant comments. Keep the code clean and focused on implementation.

Encoding Rule: All files must be written in UTF-8 (without BOM).

Line Ending Rule: All files must use CRLF line endings (\\r\\n).

JUCE String Safety: Never construct juce::String from ambiguous non-ASCII 8-bit literals. For non-ASCII text, always use explicit UTF-8 conversion (e.g., juce::String(juce::CharPointer_UTF8("\xEA..."))).
Modification Header: For every change, clearly state the File Path and the Function Name being modified.

Full Implementation: Never use placeholders or ellipses. You must provide the entire function body without any omissions.

Impact Summary: At the end of each response, provide a concise summary of the effects, specifically listing Added Features or Removed/Deprecated Functions.

4. Automated Git Workflow
Upon successful completion of any code modification, the agent must execute the following sequence without exception:

Staging: git add -A

Commit: git commit -m "<type>(<scope>): <summary>"

Use conventional commit types (e.g., feat, fix, refactor).

Push: git push

Crucial: Ensure the code is pushed to the current upstream branch immediately after the commit.

Error Handling:

If there are no changes to stage, report no changes to commit and terminate.

If commit or push fails (e.g., network issues, merge conflicts), report the error immediately and halt further automated tasks.

5. Safety and Constraints
Destructive Operations: Operations that alter git history (e.g., git reset --hard, git push --force) are strictly prohibited without explicit user authorization.

Task Atomicity: Aim for one logical commit and push per user request to maintain a clean and synchronized project history.