# Packaged feature acceptance

Source tests prove code behavior; package checks verify the deployed product.
Use the installed and extracted portable QA distributions outside a checkout.
Do not treat this checklist as evidence that a human has performed it.

| Advertised feature | Automated package/build gate | Human Windows check |
| --- | --- | --- |
| Emoji shortcodes / emoji Quick Search entries | Both payloads contain validated, source-identical `emojis/emojis.json`; shared Release runtime lookup/parser resolves `grinning` to U+1F600 in isolated temporary payloads; removing it fails despite a source-tree decoy | Enable emoji shortcodes; type `|grinning|` in Notepad and an Office-style field. Change delimiters and retry. Exclude the target process and verify literal text. Enable Show emojis in Quick Search and find grinning. |
| Local combos, groups, Search, Quick Search, import/export | Existing security/portability suite and manifest audit | Create, find, export/import, and expand a combo using the packaged app. |
| Supported variables, prompts, cursor, multiline, Unicode | Existing restricted-output tests | Date/time, input, cursor, emoji text, both multiline modes; blocked variables remain literal. |
| Installed/portable storage and distribution | Existing source/provenance/manifests and install/reinstall/uninstall/isolation gates | Installed data survives reinstall; portable Data remains local. |

For emoji acceptance, physically verify `emojis\emojis.json` in both distributions.
There must be no "Could not find the emoji list file" warning when loading it.
Current delimiters (`|`) and opt-in defaults are unchanged.

Staging exports `HEAD:emojis.json` from the pinned emojilib submodule as binary
Git blob bytes, avoiding Windows checkout newline conversion, and compares the
staged blob identity. The expected current asset is 256,089 bytes with SHA-256
`7ce5c17aed2f834a2eb6eab408eae2ed7b7115425512450412d298d8e45cd381`.
This is an audit record, not a hard-coded substitute for JSON validation.

Release builds resolve emoji data only beside the executable. Debug retains the
existing source-checkout fallback for developer workflows. CMake's
`EmojiRuntimeTests` is a test executable, never a distributed app or helper.
`Installer/TestPackagedEmojis.ps1` copies each payload and that test executable to
a uniquely named temporary directory, removes Qt SDK paths from its environment,
and runs from outside the checkout. A second run removes the packaged dictionary
and supplies a valid `Submodules/emojilib` decoy in the working directory; Release
lookup must still report the asset missing.
