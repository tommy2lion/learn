# Claude Code — Learning Path & Resources

A recommended learning path for a developer new to Claude Code CLI,
based on hands-on experience building projects with it.

---

## Learning Path

### Phase 1 — Essentials (start here)

Get productive immediately:

1. **Quickstart** — basic CLI usage, auth, first real tasks
2. **How Claude Code works** — understand the agentic loop and context
   window (explains why `/compact` matters and how sessions stay coherent)
3. **Permissions** — learn how to safely grant tool access so Claude can
   read, edit, and run commands without constant interruption

### Phase 2 — Daily Workflows

Optimize how you collaborate session to session:

1. **Slash commands** — `/compact`, `/clear`, `/memory`, `/hooks` — the
   commands you will use every session
2. **CLAUDE.md** — project-level memory that persists across sessions;
   put your coding conventions, architecture notes, and preferences here
   so every new session starts with full context
3. **Settings** — global vs. project scope, environment variables,
   permission modes
4. **Common workflows** — refactoring large codebases, automated PR
   creation, parallel sessions using git worktrees

### Phase 3 — Automation

Start delegating repetitive patterns so you stop repeating yourself:

1. **Skills** — reusable slash commands (e.g. `/commit`, `/review-pr`)
   that Claude invokes automatically when relevant
2. **Subagents** — spawn isolated agents for parallel tasks such as
   research, code review, or debugging while the main session continues
3. **Hooks** — auto-run shell commands on events (auto-format after every
   file edit, block risky operations, inject context on session start)

### Phase 4 — Integrations

Connect Claude to the tools you already use:

1. **MCP servers** — Model Context Protocol lets Claude talk directly to
   GitHub, databases, Slack, custom APIs — this is where Claude gains
   real domain superpowers
2. **IDE extension** — VS Code or JetBrains sidecar for inline Claude
   assistance without leaving your editor

### Phase 5 — Advanced (optional)

For multi-agent systems or shipping Claude as a service:

- **Agent teams** — coordinate multiple AI agents on the same task
- **Scheduled tasks & routines** — run Claude autonomously on a schedule
  or webhook trigger
- **Agent SDK** — build and deploy Claude-powered agents in
  Python / TypeScript

---

## Practical Tip

The fastest way to learn is to keep building real projects.
Each phase adds a new layer of leverage.

A good first step: add a `CLAUDE.md` file to any project you work on
regularly. Write down your coding style, key architectural decisions, and
anything you find yourself re-explaining to Claude at the start of every
session. Future sessions will start with that context already loaded.

---

## Official Documentation

| Resource | URL |
|----------|-----|
| Documentation home | https://code.claude.com/docs/en/overview.md |
| Quickstart | https://code.claude.com/docs/en/quickstart.md |
| CLI reference | https://code.claude.com/docs/en/cli-reference.md |
| Interactive mode & slash commands | https://code.claude.com/docs/en/interactive-mode.md |
| Skills | https://code.claude.com/docs/en/skills.md |
| Hooks guide | https://code.claude.com/docs/en/hooks-guide.md |
| MCP (Model Context Protocol) | https://code.claude.com/docs/en/mcp.md |
| Settings | https://code.claude.com/docs/en/settings.md |
| Changelog | https://code.claude.com/docs/en/changelog.md |

## Web App

| Resource | URL |
|----------|-----|
| Claude web app | https://claude.ai |
| Claude Code web app | https://claude.ai/code |
| Anthropic website | https://www.anthropic.com |
