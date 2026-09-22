# Quick Search appearance regression

## Reproduction and cause

Windows distributions run [80](https://github.com/jubalslone/lean-beeftext/actions/runs/35683422456)
captured the released UI/delegate with Qt 6.8.3's `windows11` style in fresh light
and dark processes. Light reproduced the field failure: selected text was visible,
but unselected name, keyword and group text disappeared into black rows. Dark was legible.

The light list/viewport palette had opaque black `Base` (`#ff000000`) with black
`Text` (`#e4000000`) and `PlaceholderText` (`#80000000`), in both Active and Inactive
groups. Highlight/HighlightedText remained a visible blue/white pair. The native
window's Base was translucent white (`#b3ffffff`), not black.

Qt's Windows 11 style makes the scroll-area viewport background role transparent
during polish. Reapplying `background-color: palette(base)` through the list's QSS
resolves that background as black. Removing that redundant background declaration
lets the native view own its surface. The frame border, search field, delegate,
system theme selection and palette text roles are unchanged; no fixed theme colors.

Pinned upstream implementation:
https://github.com/qt/qtbase/blob/v6.8.3/src/plugins/styles/modernwindows/qwindows11style.cpp
(`QWindows11Style::polish(QWidget *)`).

## Automated evidence

`PickerRenderingTests` constructs the real picker UI, production delegate and
stylesheet with fixture combos, without starting hooks or touching user settings.
Fresh processes exercise light/dark with the runner's default style and explicitly
with Windows 11 style. It captures the window and desktop, logs palette roles and
checks contrasting glyph pixels for selected/unselected names, keywords, groups,
and the search field. Delegate rendering also covers Active/Inactive palettes.
A negative control restores the old QSS declaration and must reproduce invisible
unselected text in Windows 11 light mode, proving test sensitivity to this bug.

Screenshots and logs are uploaded as `Quick-Search-rendering-evidence`. The harness
does not claim to test global shortcut handling or actual human Windows 11 use.
The default `windowsvista` style may ignore the dark hint; the explicit Windows 11
cases verify both actual light and dark palettes.

## Human Windows QA still required

For each Windows Light and Dark appearance, fully exit Lean and relaunch the QA
build (not just close its window). Open Quick Search via its existing shortcut.

- Search for several combos from different groups. Check the input text and caret.
- Check selected AND unselected combo names, keywords and group names.
- Move selection with arrows and mouse; check focus changes and insertion.
- Repeat at 100%, 125% and 150% scaling where practical; capture screenshots.
- Repeat in the installed and portable packages, outside a source checkout.

Live theme switching without relaunch is not changed by this fix.
