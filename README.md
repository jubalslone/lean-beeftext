# Lean Beeftext

Lean Beeftext is a privacy-focused, security-conscious text expander for Windows, based on Beeftext 16.0. It keeps Beeftext's fast local combo workflow while deliberately avoiding features that read sensitive system data, execute scripts, or automate arbitrary keyboard actions.

Type a short keyword and Lean Beeftext expands it into the text you use every day: signatures, addresses, boilerplate, dates, form language, and other frequently repeated text.

**New to Lean Beeftext? Start with the [5-minute User Guide](USER_GUIDE.md).**

Lean Beeftext is built on a project we genuinely like. Beeftext provides a fast, practical local text-expansion workflow; Lean Beeftext adapts that foundation for environments where privacy, predictability, and a narrower execution surface matter more than extensibility.

## Download

Download the [latest public release](https://github.com/jubalslone/lean-beeftext/releases/latest).

Two Windows distributions are available:

- **Installer** — recommended for normal use
- **Portable ZIP** — self-contained and keeps its data beside the application

The Windows executables are Authenticode-signed and RFC 3161 timestamped through Microsoft Azure Artifact Signing. Because the signing identity and application are new, Microsoft Defender SmartScreen may initially show an unfamiliar-app warning while reputation accumulates. Each release page publishes SHA-256 hashes for verification.

## Why Lean Beeftext?

[Beeftext](https://github.com/xmichelo/Beeftext) is a capable open-source text expander. Lean Beeftext keeps its practical local workflow while choosing a deliberately narrower execution model for privacy-sensitive workflows.

Lean Beeftext supports:

- Local text expansion
- Groups, in-window combo search, and Quick Search
- Dates and times
- Input prompts
- Nested combos
- Uppercase, lowercase, and trim transformations
- Caret repositioning with `#{cursor}`
- Optional real multiline snippets
- Emoji shortcodes
- Combo import and export
- Portable mode

Malformed, unknown, or deliberately blocked variable syntax remains visible as literal text instead of executing.

## Creating a Combo

For a step-by-step introduction, see the [User Guide](USER_GUIDE.md).

Open Lean Beeftext, choose **Combos > New**, then enter a keyword and snippet. For example, the keyword `;addr` could expand to:

```text
123 Example Street
Des Moines, IA 50309
```

Save the combo, then type its keyword in a normal text field. Triggering can be automatic or manual, depending on your Preferences.

### Choose a distinctive keyword

For automatic substitution, strongly prefer a keyword that starts with a character or prefix you do not normally type as part of ordinary prose. This greatly reduces accidental expansions.

A semicolon is a simple default:

- `;addr`
- `;sig`
- `;meeting`

Other useful conventions include `$sig` or `::meeting`. Pick one convention that fits the way you type and use it consistently across your combo library.

There is no universally best prefix. A semicolon is convenient for many people, while `$` or `::` may be a poor choice if you frequently write code, shell commands, or other text where those characters are common. The important part is choosing something distinctive in your own workflow. Bare ordinary words such as `address`, `thanks`, or `meeting` are much more likely to expand unintentionally when automatic substitution is enabled.

If you use manual triggering, accidental expansion is less likely, but a consistent prefix still makes combos easier to recognize, search, and remember.

**Search** (`Ctrl+F`) filters and manages combos in the main window. **Quick Search** uses its separate global shortcut to open a small searchable overlay while you work in another application, then inserts the combo you choose.

## Variables

Lean Beeftext supports the following variables:

- `#{date}` — the current local date.
- `#{time}` — the current local time.
- `#{dateTime}` — the current local date and time.
- `#{dateTime:FORMAT}` — a custom date/time format, such as `#{dateTime:yyyy-MM-dd}`.
- `#{dateTime:OFFSET:FORMAT}` — a custom format with a date/time offset, such as `#{dateTime:+1d:yyyy-MM-dd}` or `#{dateTime:+1w-2d:yyyy-MM-dd}`.
- `#{combo:keyword}` — the snippet belonging to another combo.
- `#{upper:keyword}` — another combo's snippet converted to uppercase.
- `#{lower:keyword}` — another combo's snippet converted to lowercase.
- `#{trim:keyword}` — another combo's snippet with leading and trailing whitespace removed.
- `#{input:Name}` — asks for text at expansion time using the prompt label `Name`.
- `#{cursor}` — sets the caret position after expansion.

### Custom date/time formatting

Only **`dateTime`** accepts a custom Qt format pattern. Use `#{dateTime:FORMAT}` or `#{dateTime:OFFSET:FORMAT}`.

`#{date}` and `#{time}` use your system locale's normal date or time format; forms such as `#{date:yyyy-MM-dd}` or `#{time:hh:mm}` are not supported custom-format variables.

Plain `#{dateTime}` also uses your system locale's normal long date/time format. On an English (United States) system, Qt's long date/time pattern is equivalent to:

```text
#{dateTime:dddd, MMMM d, yyyy h:mm:ss AP t}
```

For example:

```text
Tuesday, September 1, 2026 11:30:00 PM CDT
```

The exact wording and time-zone text depend on your Windows locale and time zone.

A few useful custom formats:

| Variable | Example result |
| --- | --- |
| `#{dateTime:M/d/yyyy}` | `9/1/2026` |
| `#{dateTime:MM/dd/yyyy}` | `09/01/2026` |
| `#{dateTime:MMMM d, yyyy}` | `September 1, 2026` |
| `#{dateTime:ddd, MMMM d, yyyy}` | `Tue, September 1, 2026` |
| `#{dateTime:dddd, MMMM d, yyyy}` | `Tuesday, September 1, 2026` |
| `#{dateTime:yyyy-MM-dd}` | `2026-09-01` |
| `#{dateTime:MMM d, yyyy 'at' h:mm AP}` | `Sep 1, 2026 at 11:30 PM` |
| `#{dateTime:dddd, MMMM d, yyyy h:mm:ss AP t}` | `Tuesday, September 1, 2026 11:30:00 PM CDT` |
| `#{dateTime:+1d:dddd, MMMM d, yyyy}` | `Wednesday, September 2, 2026` |

Common Qt format letters:

- `d` = day number; `dd` = zero-padded day; `ddd` = short weekday; `dddd` = full weekday
- `M` = month number; `MM` = zero-padded month; `MMM` = short month name; `MMMM` = full month name
- `yy` = two-digit year; `yyyy` = four-digit year
- `h` = hour; `hh` = zero-padded hour
- `m` = minute; `mm` = zero-padded minute
- `s` = second; `ss` = zero-padded second
- `AP` = `AM`/`PM`; `ap` = `am`/`pm`
- `t` = time-zone indicator
- `z` / `zzz` = milliseconds
- `w` / `ww` = week number (Lean Beeftext extension)

**Watch the capitalization:** `M` means **month**, while lowercase `m` means **minute**.

Date/time offsets use a separate signed syntax before the format. They may combine:

- `y` = years
- `M` = months
- `w` = weeks
- `d` = days
- `h` = hours
- `m` = minutes
- `s` = seconds
- `z` = milliseconds

For example, `#{dateTime:+1w-2d:yyyy-MM-dd}` means "five days from now, formatted as year-month-day."

Malformed, unknown, and blocked variables remain visible as literal text. Lean Beeftext does not allow combo variables to read the clipboard, read environment variables, execute PowerShell, generate arbitrary key events, generate arbitrary keyboard shortcuts, or introduce programmed delays. For example, `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{key:enter}`, `#{shortcut:Win+R}`, and `#{delay:500}` stay literal.

## Cursor Placement

An exact, case-sensitive `#{cursor}` marker is removed after nested combos, input, and text transformations have finished. If a snippet contains more than one exact marker, all exact markers are removed and the last one determines the final caret position.

Caret movement is allowed only when the text after the final marker can be counted predictably and the movement stays within the newly inserted text. If that suffix is ambiguous—for example, because it contains non-ASCII text—the markers remain literal and no caret movement is sent. Transforming a marker to another case, such as `#{CURSOR}`, makes it ordinary literal text.

## Multiline Snippets

A **line break** simply means starting the next words on a new line, like pressing **Enter**.

Preferences > Behavior offers two modes:

### Show line breaks as visible `\n` text

This is the safe default.

If a snippet contains:

```text
Hello,
Thank you
```

Lean Beeftext types:

```text
Hello,\nThank you
```

The `\n` is called **backslash-n**. It is literally two characters: a backslash (`\`) and the letter `n`. Lean Beeftext types those characters on the screen. It does **not** press Enter and does **not** move to a new line.

### Allow real line breaks

This opt-in setting actually creates the new line:

```text
Hello,
Thank you
```

This is useful in Word, Outlook, and other places where you want multi-line text.

But a real line break can behave like pressing **Enter** in some chat boxes, forms, and single-line fields. That can send a message or submit a form.

**Simple rule:** keep the safe setting if you want Lean Beeftext to show `\n` and stay on the same line. Choose **Allow real line breaks** only when you want Lean Beeftext to actually move to the next line.

## Completion Sound

If Lean Beeftext plays a sound after every completed combo and you do not want it:

1. Open **Preferences > Behavior**.
2. Turn off **Play sound on combo**.

The current public release may have this setting enabled on a fresh profile.

## Import and Export

Combo portability has one home:

- **Combos > Import Combos…**
- **Combos > Export Combos…**

Export asks whether to include the currently selected combos or all combos. Lean Beeftext exports a self-identifying, versioned bundle as human-readable UTF-8 JSON stored in a `.txt` file. The default filename is `Lean-Beeftext-Combos.txt`; groups and all combo fields needed for a faithful round trip are included, but Preferences are not.

Import accepts:

- Lean Beeftext `.txt` JSON bundles
- Legacy upstream Beeftext `.json` exports
- Legacy upstream Beeftext `.csv` exports

The text extension makes Lean bundles easy to inspect and often easier to share. There is no separate user-facing combo backup/restore workflow or Preferences export/import workflow.

## Compatibility Notes

### Windows 11 Notepad

Windows 11 Notepad can corrupt rapid Unicode insertion while spellcheck or autocorrect is enabled. Disable those Notepad features when using Lean Beeftext if this occurs. This issue has not appeared in our Microsoft Office testing.

### Elevated Applications

Windows may prevent a normal desktop application from inserting text into a higher-integrity, administrator-elevated application. This is a Windows UIPI boundary. Run Lean Beeftext and the destination at the same normal integrity level; running Lean Beeftext elevated is not recommended.

## Themes

Lean Beeftext follows the Windows light or dark appearance setting. If you change the Windows appearance setting while Lean Beeftext is already running, restart Lean Beeftext to apply the change consistently.

## Install or Use Portable

The normal Windows installer installs Lean Beeftext under `Program Files\Lean Beeftext` and creates a Start Menu shortcut. A desktop shortcut is optional and unchecked by default.

Installed Lean Beeftext stores restorable user data in `Lean Beeftext` beneath the Windows Documents known folder, including `Settings.ini`, `comboList.json`, user translations/configuration, and migration recovery copies. If OneDrive Known Folder Move redirects Documents, that folder naturally follows the redirected Documents location; Lean Beeftext does not configure OneDrive. Diagnostic logs and last-use caches remain in Lean-specific LocalAppData to avoid unnecessary document-sync churn.

On the first installed launch, Lean Beeftext can detect a registered upstream Beeftext installation or a strongly identified portable copy, import only combos and groups, and verify the saved result. If the exact selected upstream copy is running, Lean can request a graceful close and continue the same migration; it never force-terminates the source. After an installed-source import, Lean recommends manually uninstalling upstream Beeftext because running both text expanders can cause duplicate or conflicting expansions; Lean does not invoke the upstream uninstaller. Safe cleanup of a strongly identified portable copy remains a separate opt-in action and occurs only after a recovery snapshot, parse, atomic save, fresh reload, and content validation all succeed. Differing libraries are never silently merged, and legacy upstream AppData is preserved for recovery.

Uninstall removes the application and its shortcuts but preserves the entire Documents data folder, so uninstalling and reinstalling does not discard user data.

The portable package remains available. It contains `Portable.bin` and keeps settings, combos, logs, and other application data under its local `Data` folder. Keep that package in a writable location and move the folder together with the executable. Portable Lean Beeftext never uses installed Documents storage and never runs the installed migration assistant.

See [INSTALLER.md](INSTALLER.md) for exact storage paths, installer identity, migration safeguards, and the unattended installation contract.

## Security Model

Lean Beeftext intentionally limits combo evaluation and text insertion. It does not restore clipboard insertion, PowerShell execution, environment-variable reads, arbitrary key/shortcut/delay variables, Discord clipboard behavior, or upstream updater activity. Modifiers held during a substitution cause the operation to fail closed, and keyboard-hook restoration is covered by regression tests.

This is a design boundary, not a claim of formal certification or absolute security. See [SECURITY_MODEL.md](SECURITY_MODEL.md) for technical details and QA expectations.

## Project Status

Lean Beeftext 1.0.0 is publicly released for Windows as both a signed installer and a portable ZIP. Both distributions come from the same reviewed source and contain the same signed `LeanBeeftext.exe`. The release process uses a protected manual production-signing workflow, Microsoft Azure Artifact Signing, final hash/provenance generation, and real-Windows QA before publication.

There is intentionally no network updater. New releases are reviewed, signed, verified, and published explicitly rather than downloaded or installed automatically by the application.

See the [latest release](https://github.com/jubalslone/lean-beeftext/releases/latest) for downloads and release hashes.

## Upstream Project

Lean Beeftext is an unofficial fork of [Beeftext by Xavier Michelon](https://github.com/xmichelo/Beeftext), maintained by Jubal Slone. It exists because Beeftext is a strong, practical open-source text expander, and this fork explores a narrower, privacy-focused direction built on that foundation.

Upstream Beeftext translations and other retained project assets remain credited to their upstream authors and contributors.

## AI-Assisted Development

Development of the Lean Beeftext fork has been assisted by OpenAI coding and language tools. Design decisions, review, testing, and release responsibility remain with the project maintainer.

## License

Lean Beeftext remains available under the [MIT License](LICENSE), preserving Xavier Michelon's upstream copyright and license notice and identifying the fork maintainer's modifications.

The application also distributes components under their own licenses, including Qt under LGPLv3 and the MIT-licensed XMiLib and emojilib projects. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for attribution, component status, license links, and packaging notes.

## Building

The supported CI build uses:

- Windows and Visual Studio 2022
- CMake
- Qt 6.8 LTS for 64-bit MSVC 2022
- Inno Setup 7.1.0 for the installed distribution
- The recursively checked-out XMiLib and emojilib submodules

From a Visual Studio 2022 developer environment with Qt available to CMake:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The GitHub Actions Windows workflow performs the same configure, Release build, complete test run, clean-source checks, shared installed/portable staging, provenance recording, and packaged-file checksum generation. It compiles the installer, checks the installed payload cannot activate portable mode, and exercises silent install, same-version reinstall, and data-preserving uninstall. It produces both a portable QA artifact and installer from the exact same source commit.

Project links: [repository](https://github.com/jubalslone/lean-beeftext), [issues](https://github.com/jubalslone/lean-beeftext/issues), and [releases](https://github.com/jubalslone/lean-beeftext/releases).
