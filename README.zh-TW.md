# NppNodeJS

NppNodeJS 是讓 Notepad++ 直接執行 Node.js 腳本的 x64 Plugin，讓您自行撰寫 JavaScript，並與目前正在編輯的文件互動。

您可以自行撰寫 `.js` 或 `.mjs` 腳本，透過 `menu.json` 加入 Notepad++ 選單直接執行。內建的 Helper 模組提供簡單的非同步 API，可操作對話框、目前檔案、游標位置、目前行、選取區以及整份文件。

## 功能與特色

- 從自訂 Notepad++ 選單執行 `.js` 與 `.mjs` 腳本。
- 可直接在 `menu.json` 設定鍵盤快捷鍵。
- 使用可停駐的 Output Pane 顯示 `stdout` / `stderr`。
- 腳本執行期間即時顯示輸出。
- 只有實際產生輸出或錯誤時才建立／顯示 Output Pane。
- Output Pane 的樣式會跟隨目前 Scintilla 文件。
- 支援 UTF-8 輸出及 ANSI escape sequence 過濾。
- 提供原生 `alert()`、`prompt()`、`confirm()` 對話框。
- 可讀取及修改目前檔案、游標位置、目前行、選取區與全文。
- 支援 CommonJS 與 ES Module。
- **Ctrl + 左鍵**點選腳本選單項目，可在 Notepad++ 開啟腳本而不執行。
- Notepad++ 關閉時會清理正在執行的 Node.js 程序。
- **Set menu.json Path...** 可讓您選擇 `menu.json` 的位置，不需要直接修改 `Program Files` 下的檔案。
- **About NppNodeJS** 會直接開啟 [NppNodeJS GitHub repository](https://github.com/seantw/NppNodeJS)。

## 系統需求

- Windows x64
- Notepad++ x64
- **電腦必須安裝 Node.js**，且 `node` 指令必須可以從 `PATH` 執行。
  可從 [Node.js 官方網站](https://nodejs.org/) 下載安裝。

## 安裝

1. 編譯或取得 `NppNodeJS.dll`。
2. 建立 Plugin 目錄：`Notepad++\plugins\NppNodeJS\`。
3. 將 `NppNodeJS.dll` 放入該目錄。
4. 預設情況下，將 `menu.json`、`menu-helper.js` 與 `package.json` 放在 `NppNodeJS.dll` 同一目錄。
5. 將 Node.js 腳本放在 `script_folder` 指定的目錄。
6. 重新啟動 Notepad++。

典型的目錄結構如下：

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

## 檔案編碼

`menu.json` 以及您自行撰寫的所有 `.js` / `.mjs` 腳本，都必須使用 **UTF-8** 編碼儲存。請不要使用 ANSI、Big5 或其他舊式編碼儲存，尤其是檔案內含中文等非 ASCII 字元時。

## 設定 `menu.json`

`menu.json` 用來設定腳本目錄、Output Pane 標題、選單階層以及鍵盤快捷鍵。

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

### `menu.json` 的位置

預設情況下，NppNodeJS 會從 Plugin 目錄載入 `menu.json`：

```text
<Notepad++>\plugins\NppNodeJS\menu.json
```

可以從 **Plugins > NppNodeJS > Set menu.json Path...** 選擇其他 `menu.json`。選定的路徑會儲存在使用者自己的 Notepad++ Plugin 設定位置，因此不需要系統管理員權限，也不必修改 `Program Files` 下的檔案。

第一次執行 NppNodeJS 時，如果尚未設定自訂路徑，就會使用與 `NppNodeJS.dll` 位於同一目錄的 `menu.json`。

如果將 `menu.json` 移到其他位置，而腳本使用 `#menu-helper` 或 `#npp-helper` Package Import Alias，請將 `menu-helper.js` 與 `package.json` 一起放在同一個專案目錄，以保留原本的 Node.js Package Import 設定。

### `script_folder`

Node.js `.js` / `.mjs` 腳本所在的目錄。現在同時支援**絕對路徑**以及**相對於 `menu.json` 所在目錄的相對路徑**。JSON 路徑建議使用 `/`。

相對路徑範例：

```json
"script_folder": "./script"
```

如果 `menu.json` 位於：

```text
D:\Work\npp-nodejs\menu.json
```

那麼 `./script` 就會解析成：

```text
D:\Work\npp-nodejs\script\
```

也支援例如 `../scripts` 這類上一層目錄的寫法。原本的絕對路徑仍然可以使用：

```json
"script_folder": "D:/Work/npp-nodejs/script"
```

相對路徑一律以 **`menu.json` 所在目錄**為基準，不會以 Notepad++ 安裝目錄或目前工作目錄為基準。

### `output_pan_title`

腳本未執行時，Output Pane 所顯示的標題。

### `menu`

`menu` 定義 Notepad++ 選單階層。字串值代表相對於 `script_folder` 的腳本檔名。

## 在 `menu.json` 設定熱鍵

每個腳本選單項目的名稱後面，可以使用 **Tab 字元 `\t`** 加上熱鍵：

```json
"My Script\tCtrl+Shift+R": "my-script.js"
```

格式為：

```text
選單顯示名稱\t熱鍵
```

例如：

`menu` 從頂層起算最多可以設定 **3 層選單階層**。例如下面的設定有兩個頂層選單：第一個頂層下有 3 個腳本；第二個頂層下有 2 個腳本，以及一個包含 2 個腳本的子選單：

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

較簡單的熱鍵設定範例：

```json
"Script 1\tCtrl+1": "script1.js",
"Script 2\tCtrl+Shift+S": "script2.js",
"Script 3\tAlt+F8": "script3.js",
"Script 4\tCtrl+Alt+F12": "script4.js",
"Script 5\tShift+Tab": "script5.js"
```

### 支援的修飾鍵

| 修飾鍵 | 寫法 |
|---|---|
| Ctrl | `Ctrl` 或 `Control` |
| Shift | `Shift` |
| Alt | `Alt` |

### 支援的按鍵

| 按鍵 | 寫法 |
|---|---|
| 英文字母／數字 | `A`–`Z`、`0`–`9` |
| 功能鍵 | `F1`–`F24` |
| Tab | `Tab` |

注意事項：

- 熱鍵與選單名稱之間必須是 **Tab 字元 `\t`**。
- 熱鍵會透過 Windows `RegisterHotKey` 註冊，因此如果該組合已被 Windows 或其他程式使用，可能無法註冊。
- 不建議讓多個腳本使用相同熱鍵。
- **Ctrl + 左鍵**是 NppNodeJS 內建功能，不需要在 `menu.json` 中設定。

## 執行腳本

- **左鍵點選：** 執行腳本。
- **設定的鍵盤快捷鍵：** 執行對應腳本。
- **Ctrl + 左鍵點選：** 在 Notepad++ 開啟腳本，但不執行。

每次執行腳本時，Output Pane 會先清除前一次輸出並重複使用。Plugin 啟動時不會因為 Output Pane 存在而自動建立或顯示它。

## Helper API

CommonJS 腳本：

```js
const npp = require('#menu-helper');
```

ES Module 腳本：

```js
import npp from '#menu-helper';
```

### API 參考表

| API | 參數 | 回傳值 | 功能說明 |
|---|---|---|---|
| `alert(message)` | `message`: string | `Promise<void>` | 顯示 Alert，未指定標題時使用目前選單項目的名稱。 |
| `alert(message, title)` | `message`: string、`title`: string | `Promise<void>` | 顯示指定視窗標題的 Alert。 |
| `prompt(message)` | `message`: string | `Promise<string \| null>` | 顯示輸入框，預設值為空字串。按 OK 回傳輸入文字，按 Cancel 回傳 `null`。 |
| `prompt(message, defaultValue)` | `message`: string、`defaultValue`: string | `Promise<string \| null>` | 顯示輸入框，並預先填入 `defaultValue`。 |
| `prompt(message, defaultValue, title)` | `message`: string、`defaultValue`: string、`title`: string | `Promise<string \| null>` | 顯示指定視窗標題的輸入框。 |
| `confirm(message)` | `message`: string | `Promise<boolean>` | 顯示確認對話框。OK 回傳 `true`，Cancel 回傳 `false`。未指定標題時使用目前選單項目的名稱。 |
| `confirm(message, title)` | `message`: string、`title`: string | `Promise<boolean>` | 顯示指定視窗標題的確認對話框。 |
| `getFileName()` | 無 | `Promise<string>` | 取得目前文件的完整路徑。尚未儲存的文件回傳空字串。 |
| `getCursor()` | 無 | `Promise<{line: number, column: number}>` | 取得目前游標位置。`line` 與 `column` 都是 **0-based**。 |
| `getLine()` | 無 | `Promise<string>` | 取得目前行文字，不包含原本的換行字元。 |
| `setLine(text)` | `text`: string | `Promise<void>` | 取代目前行，並保留原本的換行方式。 |
| `getSelection()` | 無 | `Promise<string>` | 取得目前選取文字。沒有選取時回傳空字串。 |
| `setSelection(text)` | `text`: string | `Promise<void>` | 取代目前選取文字；若沒有選取，則在游標位置插入文字。 |
| `hasSelection()` | 無 | `Promise<boolean>` | 判斷目前是否有選取文字。 |
| `getText()` | 無 | `Promise<string>` | 取得目前文件的全部文字。 |
| `setText(text)` | `text`: string | `Promise<void>` | 取代目前文件的全部文字。 |

### 游標範例

```js
const cursor = await npp.getCursor();
console.log(cursor.line, cursor.column);
```

兩個數值都是從 `0` 開始，因此第一行是 `0`、第一欄也是 `0`。

### 文件操作範例

```js
const text = await npp.getText();
const selection = await npp.getSelection();

if (await npp.hasSelection()) {
  await npp.setSelection(selection.toUpperCase());
} else {
  await npp.setText(text + '\nAdded by NppNodeJS.');
}
```

## CommonJS 與 ES Module

CommonJS：

```js
const npp = require('#menu-helper');
```

ES Module：

```js
import npp from '#menu-helper';
```

內附的 `package.json` 提供 Package Import Alias：

```json
"imports": {
  "#menu-helper": "./menu-helper.js",
  "#npp-helper": "./menu-helper.js"
}
```

建議使用 `#menu-helper`。

## 從原始碼編譯

`build-x64.bat` 使用 MinGW / Strawberry GCC。

1. 執行 `get-cjson.bat` 取得所需的 cJSON 原始碼。
2. 確認 `g++` 已加入 `PATH`。
3. 執行 `build-x64.bat`。
4. DLL 會產生於 `build/NppNodeJS.dll`。

編譯使用 C++17，產生 Windows x64 DLL。

## License

NppNodeJS 採用 **MIT License** 授權。詳細內容請參閱 [LICENSE](LICENSE)。
