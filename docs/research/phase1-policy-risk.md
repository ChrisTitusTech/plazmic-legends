# Phase 1 EULA-risk decision

Originally reviewed on 2026-07-29. Revised on 2026-08-12 only to reference the
later Phase 7 capability model; the decision documented here remains a Phase 1
decision. This is a project-scope decision, not legal advice.

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

Phase 1 approved external, read-only process inspection, including client
symbols and the MVP state. On 2026-08-12, the owner replaced Phase 1's
product-category limits with the capability-scoped expansion model in
`SPEC.md`. A later write, injection, input, automation, scripting, remote, or
service capability requires explicit approval and validation under its own
numbered phase; this historical Phase 1 decision does not approve or reject it.

Protection bypass remains outside the project boundary. Credentials, private
chat, memory dumps, and account data remain excluded from logs and fixtures.
The project remains unaffiliated with and unsupported by Daybreak.

Package publication is authorized. Phase 2 is technically permitted after
Phase 1 proves a bounded reusable memory-reader boundary and reaches its normal
approval checkpoint.
