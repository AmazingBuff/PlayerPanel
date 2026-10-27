# asset

Binary resources used by the static analysis documents in `docs/`. Nothing here is loaded by
the plugin or shipped with it.

## `SkyrimSE.original.exe`

- Retail **Skyrim Special Edition 1.6.1170** executable, byte-identical to the game's
  `SkyrimSE.exe` (37,157,144 bytes).
- SHA-256 `C434208894F07F604B852F29B8EDC3A58C4DE63DE783373733E72B2B73F33BE9`.
- Provenance: copied read-only from `E:\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe`
  (the copy keeps the original name so the audit records can name their source).
- Purpose: reproduce the addresses and facts recorded in
  [behavior-graph-construction-audit.md](../docs/behavior-graph-construction-audit.md). All
  addresses there are RVAs. LLVM listing VAs use the PE preferred base `0x140000000 + RVA`; actual live-process VAs use the ASLR-selected module base plus RVA.

The retail `.text` is SteamStub 3.1 encrypted, so it cannot be disassembled directly. The audits
run on a locally generated unpacked copy:

```
SkyrimSE.original.exe.unpacked.exe   36,950,016 bytes
SHA-256 843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B
```

Derivation (documented in the audit's "复现" section): copy the exe to
`build/g1-audit-20261023/SkyrimSE.original.exe`, then run Steamless 3.1.0.5 CLI with
`--recalcchecksum`. The unpacked image stays under the git-ignored `build/` directory; only this
source executable is tracked.
