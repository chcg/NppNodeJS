# NppNodeJS

NppNodeJS is a Notepad++ x64 plugin for running Node.js scripts directly from Notepad++ and allowing your JavaScript code to interact with the document currently being edited.

Write your own `.js` or `.mjs` scripts, add them to `menu.json`, and run them from a Notepad++ menu. The bundled helper module provides a simple asynchronous API for dialogs, the current file, cursor position, lines, selections, and the entire document.

## Features

- Run `.js` and `.mjs` Node.js scripts from configurable Notepad++ menus.
- Configure keyboard shortcuts directly in `menu.json`.
- Persistent dockable Output Pane for `stdout` and `stderr`.
- Live output while the script is running.
- The Output Pane is created and shown only when actual output or an error is produced.
- Output Pane styling follows the active Scintilla document.
- UTF-8 output with ANSI escape-sequence filtering.
- Native `alert()`, `prompt()`, and `confirm()` dialogs.
- Read and modify the current file, cursor position, current line, selection, and complete document.
- CommonJS and ES Module support.
- **Ctrl+left-click** a script menu item to open the script in Notepad++ without executing it.
- Running Node.js processes are cleaned up when Notepad++ shuts down.
- **Set menu.json Path...** lets you choose the `menu.json` file without modifying files under `Program Files`.

## Requirements

- Windows x64
- Notepad++ x64
- **Node.js must be installed on the computer**, and the `node` command must be available in `PATH`.
  Download Node.js from the [official Node.js website](https://nodejs.org/).

## Installation

1. Build or obtain `NppNodeJS.dll`.
2. Create the plugin directory:
   `Notepad++\plugins\NppNodeJS\`
3. Copy `NppNodeJS.dll` into that directory.
4. Place `menu.json`, `menu-helper.js`, and `package.json` in the same directory as `NppNodeJS.dll` for the default setup.
5. Put your Node.js scripts in the directory specified by `script_folder`.
6. Restart Notepad++.

A typical layout is:

```text
Notepad++\
├─ plugins\
│  └─ NppNodeJS\
│     ├─ NppNodeJS.dll
│     ├─ menu.json
│     ├─ menu-helper.js
│     ├─ package.json
│     └─ script\
│        ├─ hello-world.js
│        └─ my-script.js
```

## File Encoding

`menu.json` and all `.js` / `.mjs` scripts you create for NppNodeJS must be saved as **UTF-8**. Do not save these files as ANSI, Big5, or another legacy encoding, especially when they contain non-ASCII characters.

## Configuration

`menu.json` controls the script directory, Output Pane title, menu hierarchy, and optional keyboard shortcuts.

```json
{
  "script_folder": "D:/Work/npp-nodejs/script",
  "output_pan_title": "Output Pane",
  "menu": {
    "Tools": {
      "Hello World\tCtrl+1": "hello-world.js",
      "Run Report\tCtrl+Shift+R": "report.js",
      "Open Utility\tAlt+F8": "utility.js"
    }
  }
}
```

### `menu.json` location

By default, NppNodeJS loads `menu.json` from the plugin directory:

```text
<Notepad++>\plugins\NppNodeJS\menu.json
```

You can change the `menu.json` location from **Plugins > NppNodeJS > Set menu.json Path...**. The selected path is stored in the user's Notepad++ plugin configuration area, so changing the path does not require administrator permission.

The first time NppNodeJS runs, if no custom path has been configured, it uses the `menu.json` next to `NppNodeJS.dll`.

When using a custom `menu.json` location, keep `menu-helper.js` and `package.json` in the same project directory when your scripts use the `#menu-helper` or `#npp-helper` package import aliases. This preserves the existing Node.js package import configuration.

### `script_folder`

The directory containing your `.js` and `.mjs` scripts. It supports both absolute paths and paths relative to the directory containing `menu.json`. Forward slashes are recommended in JSON paths.

Relative path example:

```json
"script_folder": "./script"
```

If `menu.json` is:

```text
D:\Work\npp-nodejs\menu.json
```

then `./script` resolves to:

```text
D:\Work\npp-nodejs\script\
```

Parent-directory paths are also supported, for example `../scripts`. Absolute paths continue to work as before:

```json
"script_folder": "D:/Work/npp-nodejs/script"
```

Relative paths are always resolved against the directory containing `menu.json`, not against Notepad++'s installation directory or the current working directory.

### `output_pan_title`

The title displayed by the dockable Output Pane when no script is running.

### `menu`

The menu object defines the menu hierarchy. A string value is a script filename relative to `script_folder`.

Menu item titles may optionally contain a tab followed by a keyboard shortcut:

```text
Menu title\tHotkey
```

The tab and shortcut are parsed by NppNodeJS and the shortcut is registered as a Windows global hotkey for the current desktop session.

## Keyboard shortcuts

Keyboard shortcuts are configured directly in the **menu item title** in `menu.json`:

```json
"My Script\tCtrl+Shift+R": "my-script.js"
```

The supported modifier names are:

| Modifier | Syntax |
|---|---|
| Control | `Ctrl` or `Control` |
| Shift | `Shift` |
| Alt | `Alt` |

The supported key forms are:

| Key | Syntax |
|---|---|
| Letter or digit | `A`–`Z`, `0`–`9` |
| Function keys | `F1`–`F24` |
| Tab | `Tab` |

Examples:

The `menu` hierarchy can contain up to **3 levels from the top level**. For example, this configuration has two top-level menus. The first contains three scripts. The second contains two scripts and one submenu, which contains two more scripts:

```json
{
  "menu": {
    "File Tools": {
      "Open File": "open-file.js",
      "Save File": "save-file.js",
      "Backup File": "backup-file.js"
    },
    "Document Tools": {
      "Format Document": "format.js",
      "Check Document": "check.js",
      "Advanced": {
        "Convert Encoding": "convert-encoding.js",
        "Remove Empty Lines": "remove-empty-lines.js"
      }
    }
  }
}
```

A shorter example of assigning shortcuts to script items is:

```json
"Script 1\tCtrl+1": "script1.js",
"Script 2\tCtrl+Shift+S": "script2.js",
"Script 3\tAlt+F8": "script3.js",
"Script 4\tCtrl+Alt+F12": "script4.js",
"Script 5\tShift+Tab": "script5.js"
```

A few points to note:

- The shortcut is separated from the visible menu text by a **tab character** (`\t`).
- The shortcut is registered by Windows through `RegisterHotKey`, so a combination already used by Windows or another application may fail to register.
- Avoid assigning the same shortcut to multiple scripts.
- `Ctrl+left-click` is a built-in NppNodeJS shortcut and does not need to be configured in `menu.json`.

## Running scripts

- **Left-click:** execute the selected script.
- **Keyboard shortcut:** execute the script assigned to that shortcut.
- **Ctrl+left-click:** open the script in Notepad++ without executing it.

The Output Pane is cleared at the beginning of an execution and reused for subsequent runs. It is not created or shown merely because the plugin starts.

## Helper API

Add the helper module to a CommonJS script with:

```js
const npp = require('#menu-helper');
```

The same helper can be imported from an ES module:

```js
import npp from '#menu-helper';
```

### API reference

| API | Parameters | Returns | Description |
|---|---|---|---|
| `alert(message)` | `message`: string | `Promise<void>` | Shows an alert using the current menu item as the default title. |
| `alert(message, title)` | `message`: string, `title`: string | `Promise<void>` | Shows an alert with a custom window title. |
| `prompt(message)` | `message`: string | `Promise<string \| null>` | Shows a prompt with an empty default value. Returns the entered string, or `null` if cancelled. |
| `prompt(message, defaultValue)` | `message`: string, `defaultValue`: string | `Promise<string \| null>` | Shows a prompt pre-filled with `defaultValue`. |
| `prompt(message, defaultValue, title)` | `message`: string, `defaultValue`: string, `title`: string | `Promise<string \| null>` | Shows a prompt with a custom window title. |
| `confirm(message)` | `message`: string | `Promise<boolean>` | Shows a confirmation dialog using the current menu item as the default title. `true` means OK; `false` means Cancel. |
| `confirm(message, title)` | `message`: string, `title`: string | `Promise<boolean>` | Shows a confirmation dialog with a custom window title. |
| `getFileName()` | none | `Promise<string>` | Returns the full path of the current document. Returns an empty string when the document has not been saved. |
| `getCursor()` | none | `Promise<{line: number, column: number}>` | Returns the current cursor position. Both `line` and `column` are **0-based**. |
| `getLine()` | none | `Promise<string>` | Returns the text of the current line without its line-ending characters. |
| `setLine(text)` | `text`: string | `Promise<void>` | Replaces the current line while preserving its original line ending. |
| `getSelection()` | none | `Promise<string>` | Returns the selected text. Returns an empty string when there is no selection. |
| `setSelection(text)` | `text`: string | `Promise<void>` | Replaces the current selection. If there is no selection, inserts the text at the cursor. |
| `hasSelection()` | none | `Promise<boolean>` | Returns whether text is currently selected. |
| `getText()` | none | `Promise<string>` | Returns the complete text of the current document. |
| `setText(text)` | `text`: string | `Promise<void>` | Replaces the complete text of the current document. |

### Cursor example

```js
const cursor = await npp.getCursor();
console.log(cursor.line, cursor.column);
```

Both values start at `0`, so the first line is `0` and the first column is `0`.

### Document example

```js
const text = await npp.getText();
const selection = await npp.getSelection();

if (await npp.hasSelection()) {
  await npp.setSelection(selection.toUpperCase());
} else {
  await npp.setText(text + '\nAdded by NppNodeJS.');
}
```

## CommonJS and ES Modules

CommonJS:

```js
const npp = require('#menu-helper');
```

ES Module:

```js
import npp from '#menu-helper';
```

The included `package.json` defines the package import alias:

```json
"imports": {
  "#menu-helper": "./menu-helper.js",
  "#npp-helper": "./menu-helper.js"
}
```

`#menu-helper` is the recommended alias.

## Building from source

`build-x64.bat` uses MinGW / Strawberry GCC.

1. Run `get-cjson.bat` to obtain the required cJSON source files.
2. Make sure `g++` is available in `PATH`.
3. Run `build-x64.bat`.
4. The DLL will be written to `build/NppNodeJS.dll`.

The build script uses C++17 and produces a Windows x64 DLL.

## License

NppNodeJS is released under the **MIT License**. See [LICENSE](LICENSE).
