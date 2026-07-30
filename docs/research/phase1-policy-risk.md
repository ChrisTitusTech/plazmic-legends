# Phase 1 EULA-risk decision

Reviewed on 2026-07-29. This is a conservative project-scope decision, not
legal advice.

## Official sources

- [Daybreak Terms of Service](https://www.daybreakgames.com/terms-of-service?locale=en_US)
  prohibit reverse engineering, disassembly, unauthorized software that
  modifies gameplay or provides an advantage, and distributing related
  utilities without express written permission.
- [Daybreak End User License Agreement](https://www.daybreakgames.com/eula?locale=en_US)
  restricts attempts to discover source code, algorithms, file formats, or
  programming interfaces without advance written permission, except where
  applicable law expressly overrides that restriction.
- [EverQuest Rules of Conduct](https://help.daybreakgames.com/hc/en-us/articles/230629007-What-are-the-EverQuest-Rules-of-Conduct)
  also state that related utilities or software tools may not be posted or
  distributed without express written permission.

No official EverQuest Legends exception, tool API, or written authorization
for this project was found.

## Explicit development decision

The project owner directed development to continue on 2026-07-29 despite these
restrictions. This project therefore knowingly operates against the Daybreak
EULA and related published rules. That statement records accepted development
risk; it does not grant permission, waive Daybreak's rights, prevent account
action, or provide legal advice.

Private development may continue with external, read-only process inspection,
including client symbols and the approved MVP state. The project remains
unaffiliated with and unsupported by Daybreak.

The following boundaries remain mandatory:

- no game-memory or client-file writes;
- no code injection, DLL override, DirectX hook, or client patch;
- no authentication, integrity, anti-cheat, or protection bypass;
- no gameplay input, automation, macros, scripting, or remote control;
- no credentials, chat, memory dumps, or account data in logs or fixtures; and
- no push, public package, release, or distribution without a separate
  explicit decision.

The branch remains local. Phase 2 is technically permitted after Phase 1
proves a bounded reusable memory-reader boundary and reaches its normal
approval checkpoint.
