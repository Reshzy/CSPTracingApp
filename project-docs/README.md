# TracingApp prototype planning package

Prepared 2026-09-25. These are implementation instructions, not a completed or tested application.

- [MASTER.md](MASTER.md): prototype contract, architecture, implementation order, acceptance gates, and risks.
- [CURSOR_PROMPTS.md](CURSOR_PROMPTS.md): ordered, copy/paste implementation tasks with bounded changes and verification.
- [VALIDATION.md](VALIDATION.md): execution record and manual test checklist. All results start NOT RUN.

Copy `project-docs/` into the root of a new TracingApp implementation repository. Start with prompt 01. Preserve any existing repository instructions. Do not implement inside synced `sources/` folders.

The first deliverable is a working capture/overlay/exclusion experiment. Automatic canvas synchronization comes only after that experiment passes. Read the master before executing prompts. Each prompt incorporates the shared execution contract printed at the beginning of CURSOR_PROMPTS.md.

No specific OpenCV major version, OBS backend compatibility, CSP accessibility capability, installed compiler, or runtime performance is assumed proven. Record exact installed versions and measured outcomes in VALIDATION.md.
