# NppNodeJS

NppNodeJS 是 Notepad++ 的 x64 外掛程式。它可自訂腳本選單，並從選單執行 Node.js 腳本。



## 功能與特色

- 在「輸出面板」即時顯示 `stdout` 和 `stderr`。
- 透過 JavaScript 讀取或修改 Notepad++ 當前編輯中的「全文」、「所在列」和「選取文字」，還可取得「檔案路徑」及「游標位置」。
- 提供原生 `npp.alert()`、`npp.prompt()`、`npp.confirm()` 對話框。
- 內建 Helper 模組，支援 CommonJS 和 ES Module；預設為同步 API，也提供 Promise 非同步 API。
- 按住 Ctrl 並點擊腳本選單項目，可在 Notepad++ 開啟腳本進行編輯；檔案不存在時可選擇建立範本。
- 關閉 Notepad++ 時會清理正在執行的 Node.js 程序。

## 系統需求

- Windows x64
- Notepad++ x64
- Node.js，且 `node` 指令可從 `PATH` 執行。

## 安裝

1. 編譯或取得 `NppNodeJS.dll`。
2. 建立 Plugin 目錄：`Notepad++\plugins\NppNodeJS\`。
3. 將 `NppNodeJS.dll` 放入該目錄。
4. 預設情況下，將 `menu.json`、`menu-helper.js` 與 `package.json` 放在 `NppNodeJS.dll` 同一目錄。
5. 將 `.js` 或 `.mjs` 腳本放在 `script_folder` 指定的目錄。
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


## 設定 `menu.json`

`menu.json` 用來設定腳本目錄、Output Pane 標題、選單階層以及鍵盤快捷鍵。您可以直接從 Notepad++ 主選單 **Plugins > NppNodeJS > Open menu.json** 開啟它。它長得像這樣：

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

- `menu.json` 必須使用 UTF-8 編碼，預設會從 `NppNodeJS.dll` 所在目錄載入。若要使用其他位置，可透過 **Plugins > NppNodeJS > Set menu.json Path...** 設定，便不必直接修改需要系統管理員權限的 `Program Files` 目錄。
- `script_folder`：腳本目錄。可使用絕對路徑，或以 `menu.json` 所在目錄為基準的相對路徑，例如 `./script`、`../scripts`。
- 若更動了 `script_folder`，請將 `menu-helper.js` 和 `package.json` 拷到該目錄的上一層，讓 Node.js 能找到 Package Import Alias。
- `output_pan_title`：輸出面板標題。
- `menu`：定義可巢狀的腳本選單；每個腳本選單項目對應一個 `.js` 或 `.mjs` 檔名。
- 修改並儲存 `menu.json` 後，選擇 **Plugins > NppNodeJS > Rebuild Menu** 即可重建腳本選單。



### 鍵盤快捷鍵

每個「選單項目名稱」後面，可以使用 **Tab 字元 `\t`** 加上快捷按鍵：

```json
"My Script\tCtrl+Shift+R": "my-script.js"
```

支援的修飾鍵：

| 修飾鍵 | 寫法 |
|---|---|
| Ctrl | `Ctrl` 或 `Control` |
| Shift | `Shift` |
| Alt | `Alt` |

支援的按鍵：

| 按鍵 | 寫法 |
|---|---|
| 英文字母／數字 | `A`–`Z`、`0`–`9` |
| 功能鍵 | `F1`–`F24` |
| Tab | `Tab` |

快捷鍵必須至少包含一個修飾鍵，例如 `Ctrl+1`、`Alt+F8` 或 `Shift+Tab`；單獨的 `Tab` 不會註冊。熱鍵透過 Windows `RegisterHotKey` 註冊，如果組合已被 Windows 或其他程式占用，就可能無法使用。


## 撰寫您的 JavaScript

您的 JavaScript 檔案請以 **UTF-8** 編碼儲存，尤其是含有中文等非 ASCII 字元時。

初次安裝此外掛後，可從 **Plugins > NppNodeJS > Open Scripts Folder** 開啟內附的腳本範例，或按住 Ctrl 並點擊腳本選單項目，直接開啟範例原始碼。

### Helper API



#### CommonJS 與 ES Module

```js
/* CommonJS (.js) */
const npp = require('#menu-helper');
```
或：
```js
/* ES Module (.mjs) */
import npp from '#menu-helper';
```

### Promise 非同步 API
預設 Helper API 是同步 (Sync) 版本。如需非同步 (Async) 工作，請使用 Promise API。各 Method 回傳值會以 Promise 包裝。

```js
/* CommonJS (.js) */
const npp = require('#menu-helper').promises;
```
或：
```js
/* ES Module (.mjs) */
import nppModule from '#menu-helper';
const npp = nppModule.promises;
```
非同步範例請參考 `script/test-async-alert.js`。

**注意事項：**
- 在同一支腳本中不可同時混用「同步」與「非同步」。
- 腳本第一次呼叫 Helper API 時，外掛會檢查 `menu-helper.js` 與 `NppNodeJS.dll` 是否相容；若不相容，會顯示錯誤訊息。
- 若您曾變更 `script_folder` 的預設路徑，日後更新 `NppNodeJS.dll` 時，請一併更新 `menu-helper.js`，以確保兩者相容。新版 `menu-helper.js` 可從 `Notepad++\plugins\NppNodeJS` 目錄取得。

### API 文件

| API | 回傳值 | 功能 |
|---|---|---|
| `alert(message, [title])` | `void` | 訊息對話框 |
| `prompt(message, [defaultValue], [title])` | `string / null` | 提示使用者輸入文字；按取消時回傳 `null` |
| `confirm(message, [title])` | `true / false` | 確認對話框 |
| `getLine()`<br>`setLine(text)` | `string / boolean` | 「取得/取代」游標所在列文字 |
| `getSelection()`<br>`setSelection(text)`<br>`hasSelection()` | `string / boolean / boolean` | 「取得/取代」目前選取文字；若沒有選取，則在游標處插入文字 |
| `getText()`<br>`setText(text)` | `string / boolean` | 「取得/取代」當前文件全部文字 |
| `getFileName()` | `string` | 取得目前文件完整路徑 |
| `getCursor()` | `{line: number, column: number}` | 取得目前游標位置 **0-based** |


## 編譯 NppNodeJS.dll

`build-x64.bat` 使用 MinGW / Strawberry GCC。

1. 執行 `get-cjson.bat` 取得所需的 cJSON 原始碼。
2. 確認 `g++` 已加入 `PATH`。
3. 執行 `build-x64.bat`。
4. 產出 DLL 於 `build/NppNodeJS.dll`。

## License

NppNodeJS 採用 **MIT License** 授權。詳細內容請參閱 [LICENSE](LICENSE)。
