# C / Arduino Naming & Consistency Audit (reusable)

A portable runbook for auditing and fixing naming + consistency in any C / C++ /
Arduino / ESP project. Copy this file into the project, fill in the
**project specifics** blocks, and work top to bottom. Nothing below is tied to one
codebase — the concrete examples are in `[brackets]` and meant to be replaced.
No Migration / fallback needed!

---

## 1. Conventions to enforce

Pick one convention per category and apply it everywhere. The defaults below are the
common Arduino/C++ choice; if the project already leans another way, match the
dominant existing style instead of fighting it.

| Category | Convention | Examples |
|---|---|---|
| Variables, struct/class fields, functions | `camelCase` *(or pick `snake_case` and be consistent)* | `targetWeight`, `loadConfig()` |
| Types: `struct` / `class` / `enum` / `typedef` | `PascalCase` | `Config`, `MotorState` |
| Macros, enum constants, true compile-time constants | `UPPER_SNAKE` | `MAX_RETRIES`, `STATE_IDLE` |
| **Mutable** globals | same case as variables — **never** `UPPER_SNAKE` (reserve that for constants/macros) | `serverActive` not `SERVER_ACTIVE` |
| Source files | `snake_case`, grouped by a domain prefix | `net_wifi.ino`, `ui_dialogs.ino` |

Quality rules (independent of case):
- A name should reflect **domain meaning**, not implementation trivia.
- One identifier = one meaning. If a word means three things (a config block, a
  selector, an index…), split it into three clear names.
- Make different concepts **clearly** different (avoid `count` vs `counter` for
  unrelated things).
- Avoid over-abbreviation (`buff`, `cfg`, `tmp`) unless it's an established idiom
  (`idx`, `len`, `pin`).
- Serialized data (JSON / EEPROM / config files) gets **human-readable, consistently
  cased, grouped** keys. Internal field names and serialized keys MAY differ — that's
  fine, but document the mapping in the load/save layer.

### Do NOT rename (externally required)
- `setup()`, `loop()`, and any library/framework API.
- ISR / callback names referenced by generated or library code
  (e.g. LVGL `*_event_cb`, `my_disp_flush`, attachInterrupt handlers).
- Generated files (UI generators like SquareLine `ui*.c/.h`, `lv_conf.h`).
- Framework-mandated filenames: the main sketch `.ino` **must** match its folder;
  `build_opt.h` is read by arduino-cli.
- External contract strings unless you intend to change the contract **and** update
  every consumer in lock-step: HTTP routes, serialized keys, CLI flags, MQTT topics,
  BLE characteristic names, protocol tokens.

---

## 2. Build a rename map first

Before editing anything, list every rename as `old -> new`, grouped by category. This
map is the single source of truth for both the edits and the deny-list in step 4.

```
# >>> PROJECT SPECIFICS: fill in <<<
types:      [OldType -> NewType]
fields:     [old_field -> newField]
functions:  [readThing -> loadThing]
globals:    [SERVER_ON -> serverOn]
macros:     [OLD_MAX -> NEW_MAX]
json keys:  [legacyKey -> goodKey]
filenames:  [old_file.ino -> new_file.ino]
routes/contracts (rename ONLY with consumers): [/oldRoute -> /newRoute]
```

---

## 3. Apply renames safely

- **Unique identifiers** → `sed` with word boundaries across source globs:
  ```bash
  sed -i 's/\bOLD\b/NEW/g' *.ino *.h *.cpp *.c
  ```
- **Prefix collisions** → rename the **longest** token first; `\b` stops
  `fooBar` from being hit while renaming `foo`. Test the regex with `rg` before `sed`.
- **Context-sensitive tokens** (same word, different meanings) → never blanket-replace;
  edit each site by hand.
- **String literals that are contracts** (routes, serialized keys, CLI flags) → a
  bare `sed` of an identifier can silently rewrite them. After any broad `sed`,
  re-check routes/keys (step 4c). If you *do* change a contract, change its consumer
  in the same commit.
- **Filenames** → `git mv old new` (preserves history), then update `#include`s,
  build scripts, and docs that name the file. In Arduino, `.ino` files are
  concatenated by folder, so they don't `#include` each other and renaming one needs
  no code change — only doc/reference updates.
- **No backward-compat shims** unless required: old serialized files will fall back
  to defaults, so a serialized-schema rename is a **breaking change** — note it in the
  changelog/release.

---

## 4. Verification runbook

Run from the project root. Replace `OLD1|OLD2|...` with the *old* side of your rename
map. `rg` = ripgrep. Each search should return **nothing** except documented
intentional hits.

**a) Deny-list — no old token reappears** (exclude generated/build output):
```bash
rg -n --glob '!build/**' --glob '!**/generated/**' 'OLD1|OLD2|OLD3'
```
Write down the allowed hits for your project (the audit doc itself; any deliberate
legacy vocabulary). Anything else is a regression.

**b) Convention spot-checks:**
```bash
# leftover snake_case fields in a camelCase project (adapt the accessor/struct name):
rg -n '(config|state)\.[a-z]+_[a-z]'
# mutable globals declared in UPPER_SNAKE (should be camelCase):
rg -n '^(bool|int|long|float|double|char|uint\w+) +[A-Z][A-Z0-9_]+ *=' *.ino *.cpp
```

**c) External contracts stay matched** (the highest-value check after a broad rename):
- HTTP routes ↔ client calls: `rg -n 'server\.on\("' ` vs `rg -n "fetch\('/|open\(.*'/" <web dir>`
- Serialized keys: the writer (UI/tool/EEPROM struct) ↔ the reader (load/save). List
  both key sets and diff them.
- CLI flags ↔ docs/help text. BLE/MQTT names ↔ peers.

**d) Serialized data still parses** (the build often won't validate it). JSON example:
```bash
python -c "import json,glob; [json.load(open(f,encoding='utf-8')) for f in glob.glob('**/*.json',recursive=True)]; print('JSON OK')"
```

**e) It compiles:**
```bash
# >>> PROJECT SPECIFICS <<<  e.g. arduino-cli compile, pio run, or your wrapper:
[ build / compile command ]
```

**f) Generated/derived artifacts regenerated from source** — rebuild, then grep the
build output for old tokens to confirm nothing stale was shipped from a cache.

---

## 5. Filename review

- Source files: one case (snake_case is typical for C/Arduino), grouped by a domain
  prefix so related files sort together (`net_*`, `ui_*`, `display_*`, `motor_*`).
- A file's name should match its **responsibility**; if a file grew to own more than
  its name implies, rename it (e.g. a `config.*` that also stores other records →
  `storage.*`).
- Keep sibling files structurally parallel (`fs_mount` + `fs_sync`, not
  `flash_filesystem` + `fs_sync`).
- Never rename framework-mandated names (sketch `.ino`, generator output, `lv_conf.h`,
  `build_opt.h`).

---

## 6. One-time setup to make this repeatable

- Drop a filled-in copy of step 4's deny-list command into the project's agent/contributor
  doc so future changes get re-checked.
- Add the compile + JSON-parse checks to CI if available.
