# NppNodeJS

NppNodeJS is an x64 Notepad++ plugin. It lets you customize script menus and run Node.js scripts from those menus.

## Features

- Displays `stdout` and `stderr` in the Output Pane in real time.
- Lets JavaScript read and modify the full text, current line, and selected text in the active Notepad++ document, and retrieve its file path and cursor position.
- Provides native `npp.alert()`, `npp.prompt()`, and `npp.confirm()` dialogs.
- Includes a helper module that supports CommonJS and ES modules. The default API is synchronous, with a Promise-based asynchronous API also available.
- Hold Ctrl and click a script menu item to open the script in Notepad++ for editing. If the file does not exist, you can choose to create a template.
- Running Node.js processes are cleaned up when Notepad++ shuts down.

## Requirements

- Windows x64
- Notepad++ x64
- Node.js, with the `node` command available on your `PATH`.

## Installation

1. Build or obtain `NppNodeJS.dll`.
2. Create the plugin directory:
   `Notepad++\plugins\NppNodeJS\`
3. Copy `NppNodeJS.dll` into that directory.
4. Place `menu.json`, `menu-helper.js`, and `package.json` in the same directory as `NppNodeJS.dll` for the default setup.
5. Put your `.js` or `.mjs` scripts in the directory specified by `script_folder`.
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

## Configure `menu.json`

`menu.json` configures the script directory, Output Pane title, menu hierarchy, and keyboard shortcuts. You can open it from **Plugins > NppNodeJS > Open menu.json** in the Notepad++ main menu. For example:

```json
{
  "script_folder": "./script",
  "output_pan_title": "Output Pane",
  "menu": {
    "Tools": {
      "Hello World\tCtrl+1": "hello-world.js",
      "Run Report\tCtrl+Shift+R": "report.js",
      "Open Utility\tAlt+F8": "utility.js"
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

- `menu.json` must use UTF-8 encoding. By default, it is loaded from the directory containing `NppNodeJS.dll`. To use a different location, choose **Plugins > NppNodeJS > Set menu.json Path...**; this avoids editing files in the `Program Files` directory, which requires administrator permission.
- `script_folder`: The script directory. Use an absolute path or a relative path based on the directory containing `menu.json`, such as `./script` or `../scripts`.
- If you change `script_folder`, copy `menu-helper.js` and `package.json` to the directory above it so Node.js can resolve the package import alias.
- `output_pan_title`: The Output Pane title.
- `menu`: Defines nested script menus. Each script menu item maps to a `.js` or `.mjs` filename.
- After editing and saving `menu.json`, choose **Plugins > NppNodeJS > Rebuild Menu** to rebuild the script menu.

### Keyboard shortcuts

Add a shortcut to a menu item by inserting a **tab character `\t`** after its title:

```json
"My Script\tCtrl+Shift+R": "my-script.js"
```

Supported modifier keys:

| Modifier | Syntax |
|---|---|
| Control | `Ctrl` or `Control` |
| Shift | `Shift` |
| Alt | `Alt` |

Supported keys:

| Key | Syntax |
|---|---|
| Letter or digit | `A`–`Z` or `0`–`9` |
| Function keys | `F1`–`F24` |
| Tab | `Tab` |

Every shortcut must include at least one modifier key. Examples include `Ctrl+1`, `Alt+F8`, and `Shift+Tab`. A shortcut consisting only of `Tab` is not registered. Windows registers shortcuts through `RegisterHotKey`, so a combination already in use by Windows or another application may not be available.

## Writing JavaScript

Save your JavaScript files as **UTF-8**, especially when they contain non-ASCII characters.

After installing the plugin, choose **Plugins > NppNodeJS > Open Scripts Folder** to open the included script examples. You can also hold Ctrl and click a script menu item to open its source in Notepad++.

### Helper API

#### CommonJS and ES Modules

```js
/* CommonJS (.js) */
const npp = require('#menu-helper');
```
Or:

```js
/* ES Module (.mjs) */
import npp from '#menu-helper';
```

### Promise-based asynchronous API

The default Helper API is synchronous. For asynchronous work, use the Promise API. Each method's return value is wrapped in a Promise.

```js
/* CommonJS (.js) */
const npp = require('#menu-helper').promises;
```
Or:

```js
/* ES Module (.mjs) */
import nppModule from '#menu-helper';
const npp = nppModule.promises;
```

See `script/test-async-alert.js` for an asynchronous example.

**Notes:**

- Do not use the synchronous and asynchronous APIs together in the same script.
- NppNodeJS checks compatibility between `menu-helper.js` and `NppNodeJS.dll` when a script first calls a Helper API. An error is shown if they are incompatible.
- If you changed the default `script_folder`, update `menu-helper.js` whenever you update `NppNodeJS.dll` to keep them compatible. You can find the latest `menu-helper.js` in `Notepad++\plugins\NppNodeJS`.

### API reference

| API | Returns | Description |
|---|---|---|
| `alert(message, [title])` | `void` | Shows a message dialog. |
| `prompt(message, [defaultValue], [title])` | `string / null` | Prompts the user for text. Returns `null` when cancelled. |
| `confirm(message, [title])` | `true / false` | Shows a confirmation dialog. |
| `getLine()`<br>`setLine(text)` | `string / boolean` | Gets or replaces the current line. |
| `getSelection()`<br>`setSelection(text)`<br>`hasSelection()` | `string / boolean / boolean` | Gets or replaces the selected text. If no text is selected, `setSelection()` inserts text at the cursor. |
| `getText()`<br>`setText(text)` | `string / boolean` | Gets or replaces all text in the current document. |
| `getFileName()` | `string` | Returns the full path of the current document. |
| `getCursor()` | `{line: number, column: number}` | Returns the current cursor position (**0-based**). |

## Building NppNodeJS.dll

`build-x64.bat` uses MinGW / Strawberry GCC.

1. Run `get-cjson.bat` to download the required cJSON source files.
2. Make sure `g++` is available on your `PATH`.
3. Run `build-x64.bat`.
4. The DLL is created at `build/NppNodeJS.dll`.


## License

NppNodeJS is released under the **MIT License**. See [LICENSE](LICENSE).
