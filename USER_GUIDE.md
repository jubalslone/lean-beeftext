# Lean Beeftext User Guide

This guide is for everyday use. If you just want to make Lean Beeftext save you some typing, start here.

## 1. Make your first combo

A **combo** has two main parts:

- **Keyword**: the short thing you type
- **Snippet**: the longer text Lean Beeftext inserts for you

Example:

- Keyword: `;sig`
- Snippet: your normal email signature

To create one:

1. Open Lean Beeftext.
2. Choose **Combos > New**.
3. Enter your snippet.
4. Enter a keyword such as `;sig`.
5. Choose a group.
6. Click **OK**.
7. Open Word, Outlook, Notepad, or another normal text box and type `;sig`.

Lean Beeftext should replace the keyword with your snippet.

## 2. Use a distinctive keyword

For automatic substitution, start your keywords with something you do not normally type in ordinary writing.

A semicolon is a good default:

- `;sig`
- `;addr`
- `;phone`
- `;followup`
- `;meeting`

Other people may prefer prefixes such as `$` or `::`.

The exact symbol does not matter as much as being consistent. A distinctive prefix makes accidental expansions much less likely.

Bare words such as `address`, `thanks`, or `meeting` are more likely to trigger when you did not mean them to.

## 3. Find a combo you forgot

You do not have to remember every keyword forever.

### Search

Press **Ctrl+F** in the main Lean Beeftext window to filter your combos.

### Quick Search

Quick Search opens a small search window while you are working in another application. Search for the combo you want, choose it, and Lean Beeftext inserts it for you.

You can see or change the Quick Search shortcut in Preferences.

## 4. Edit, disable, or delete a combo

Select the combo in the main Lean Beeftext window.

- **Edit** it when you want to change the keyword or snippet.
- **Disable** it when you want to keep it but stop it from triggering for now.
- **Delete** it when you no longer want it.

A disabled combo stays in your library and can be enabled again later.

Groups can also be disabled. A combo inside a disabled group will not trigger until that group is enabled again.

## 5. A few useful examples

### Address

Keyword:

```text
;addr
```

Snippet:

```text
123 Example Street
Des Moines, IA 50309
```

### Today's date

Keyword:

```text
;date
```

Snippet:

```text
#{date}
```

### Ask for a name when the combo runs

Keyword:

```text
;hello
```

Snippet:

```text
Hello #{input:Name},
```

Lean Beeftext will ask for the name, then insert it into the snippet.

### Put the cursor where you want to keep typing

Keyword:

```text
;letter
```

Snippet:

```text
Hello,

#{cursor}

Thank you,
```

After the combo expands, the cursor will be placed at `#{cursor}` so you can keep typing there.

For the full variable list, see [Variables in the README](README.md#variables).

## 6. Line breaks: the simple version

A **line break** just means **start the next words on a new line**, like pressing **Enter**.

For example, this has a real line break:

```text
Hello,
Thank you for your email.
```

Lean Beeftext has two ways to handle line breaks in snippets.

### Safe default: show `\n` instead

With the default setting, Lean Beeftext does **not** make a real new line.

If your snippet contains:

```text
Hello,
Thank you
```

Lean Beeftext types:

```text
Hello,\nThank you
```

The `\n` is just visible text. It does **not** press Enter and it does **not** move to the next line.

### Optional: allow real line breaks

If you turn on **Allow real line breaks**, Lean Beeftext actually makes the new line:

```text
Hello,
Thank you
```

This is useful in Word, Outlook, and other places where you want multi-line text.

But there is one important catch: in some chat boxes, forms, and single-line fields, a real line break can behave like pressing **Enter**. That can send a message or submit a form.

**Simple rule:** Leave the safe default on unless you know you want your combo to create real new lines.

## 7. Preferences you may actually care about

Open **Preferences** when you want to change how Lean Beeftext behaves.

A few useful settings:

- **Automatic or manual substitution**: choose whether Lean expands keywords automatically or only when you trigger them manually.
- **Play sound when completing a combo**: if Lean Beeftext makes a sound after every combo and you do not want it, turn this setting off.
- **Multiline snippets**: choose between visible `\n` text and real line breaks.
- **Quick Search shortcut**: choose the keyboard shortcut for Quick Search.

### Hearing a sound after every combo?

Go to **Preferences > Behavior** and turn off:

**Play sound when completing a combo**

The current public release may have that setting enabled on a fresh profile.

## 8. Import and export combos

Use:

- **Combos > Export Combos…** to save combos to a file
- **Combos > Import Combos…** to bring combos in from a file

Lean Beeftext exports combos and groups. It does not export all of your Preferences.

The normal Lean Beeftext export file is named:

`Lean-Beeftext-Combos.txt`

Lean Beeftext can also import legacy Beeftext JSON and CSV exports.

## 9. Quick troubleshooting

### Nothing expands

Make sure Lean Beeftext is running.

Also confirm that:

- the combo is enabled
- its group is enabled
- you are typing the correct keyword
- your automatic/manual substitution setting matches how you are trying to trigger it

### A combo triggers when I did not mean it to

Use a more distinctive keyword, such as `;something` instead of an ordinary word.

### I hear a sound after every combo

Go to **Preferences > Behavior** and turn off **Play sound when completing a combo**.

### I forgot the keyword

Use **Search** in the main window or **Quick Search**.

### Text looks strange in Windows 11 Notepad

Windows 11 Notepad can sometimes interfere with rapid Unicode insertion while spellcheck or autocorrect is enabled. If this happens, turn off those Notepad features.

### Lean works in normal apps but not in an administrator window

Windows can block a normal application from typing into an application running as administrator. Run both applications at the same normal permission level.

## 10. Want the technical details?

The main [README](README.md) documents variables, installation, portability, compatibility, security design, and development.

More detailed documents:

- [Security model](SECURITY_MODEL.md)
- [Installer and storage](INSTALLER.md)
- [Artifact signing](ARTIFACT_SIGNING.md)
