'use strict';

const fs = require('node:fs');

// Keep this synchronized with HELPER_API_ID in src/main.cpp.
const HELPER_API_ID = '1';
const HELPER_API_CHECK_PREFIX = '\x1eNPPNODE_HELPER_API:';
const ALERT_PREFIX = '\x1eNPPNODE_ALERT:';
const PROMPT_PREFIX = '\x1eNPPNODE_PROMPT:';
const CONFIRM_PREFIX = '\x1eNPPNODE_CONFIRM:';
const GETFILE_PREFIX = '\x1eNPPNODE_GETFILE:';
const GETCURSOR_PREFIX = '\x1eNPPNODE_GETCURSOR:';
const GETLINE_PREFIX = '\x1eNPPNODE_GETLINE:';
const SETLINE_PREFIX = '\x1eNPPNODE_SETLINE:';
const GETSELECTION_PREFIX = '\x1eNPPNODE_GETSELECTION:';
const SETSELECTION_PREFIX = '\x1eNPPNODE_SETSELECTION:';
const HASSELECTION_PREFIX = '\x1eNPPNODE_HASSELECTION:';
const GETTEXT_PREFIX = '\x1eNPPNODE_GETTEXT:';
const SETTEXT_PREFIX = '\x1eNPPNODE_SETTEXT:';

let stdinBuffer = '';
let stdinReady = false;
let stdinWaiter = null;
let syncStdinBuffer = '';
let protocolMode = null;
let helperApiChecked = false;

function ensureHelperApiCompatibility() {
  if (helperApiChecked) return;

  const idArgIndex = process.argv.indexOf('--nppnode-helper-api-id');
  const pluginApiId = idArgIndex >= 0 ? process.argv[idArgIndex + 1] : undefined;
  if (pluginApiId !== HELPER_API_ID) {
    throw new Error(
      `NppNodeJS menu-helper.js (API ID ${HELPER_API_ID}) is incompatible with ` +
      `the loaded NppNodeJS.dll (API ID ${pluginApiId || 'missing'}). ` +
      'Copy the matching menu-helper.js from Notepad++\\plugins\\NppNodeJS ' +
      'to the directory above script_folder.'
    );
  }

  fs.writeSync(1, `${HELPER_API_CHECK_PREFIX}${HELPER_API_ID}\n`, null, 'utf8');
  helperApiChecked = true;
}

function selectProtocolMode(mode) {
  if (protocolMode && protocolMode !== mode) {
    throw new Error('Do not mix the synchronous and promises menu-helper APIs in one script');
  }
  protocolMode = mode;
}

function getDefaultAlertTitle() {
  const args = process.argv;
  const index = args.indexOf('--nppnode-menu-title-b64');
  if (index >= 0 && index + 1 < args.length) {
    try {
      return Buffer.from(args[index + 1], 'base64').toString('utf8');
    } catch (_) {
      // Fall through to the environment variable.
    }
  }
  return String(process.env.NPPNODE_MENU_TITLE || 'NppNodeJS');
}

function finishStdinWaiter() {
  if (!stdinWaiter) return false;
  const newline = stdinBuffer.indexOf('\n');
  if (newline < 0) return false;

  let line = stdinBuffer.slice(0, newline);
  stdinBuffer = stdinBuffer.slice(newline + 1);
  if (line.endsWith('\r')) line = line.slice(0, -1);

  const waiter = stdinWaiter;
  stdinWaiter = null;
  process.stdin.pause();
  if (typeof process.stdin.unref === 'function') process.stdin.unref();
  waiter.resolve(line);
  return true;
}

function installStdinReader() {
  if (stdinReady) return;
  stdinReady = true;
  process.stdin.setEncoding('utf8');
  process.stdin.on('data', (chunk) => {
    stdinBuffer += String(chunk);
    finishStdinWaiter();
  });
  process.stdin.on('error', (error) => {
    if (stdinWaiter) {
      const waiter = stdinWaiter;
      stdinWaiter = null;
      waiter.reject(error);
    }
  });
}

function readResponseLine() {
  installStdinReader();

  if (finishStdinWaiter()) {
    // The function above resolves an existing waiter, so this branch is only
    // relevant when a future refactor leaves one behind.
  }

  const newline = stdinBuffer.indexOf('\n');
  if (newline >= 0) {
    let line = stdinBuffer.slice(0, newline);
    stdinBuffer = stdinBuffer.slice(newline + 1);
    if (line.endsWith('\r')) line = line.slice(0, -1);
    return Promise.resolve(line);
  }

  return new Promise((resolve, reject) => {
    stdinWaiter = { resolve, reject };
    if (typeof process.stdin.ref === 'function') process.stdin.ref();
    process.stdin.resume();
  });
}

function requestResponse(payload) {
  try {
    selectProtocolMode('promises');
    ensureHelperApiCompatibility();
  } catch (error) {
    return Promise.reject(error);
  }
  installStdinReader();
  if (stdinWaiter) {
    return Promise.reject(new Error('NppNodeJS protocol request is already pending'));
  }

  if (typeof process.stdin.ref === 'function') process.stdin.ref();
  process.stdin.resume();

  return new Promise((resolve, reject) => {
    stdinWaiter = { resolve, reject };
    process.stdout.write(payload, (error) => {
      if (error) {
        const waiter = stdinWaiter;
        stdinWaiter = null;
        process.stdin.pause();
        if (typeof process.stdin.unref === 'function') process.stdin.unref();
        if (waiter) waiter.reject(error);
      }
    });
  });
}

function requestResponseSync(payload) {
  selectProtocolMode('sync');
  ensureHelperApiCompatibility();
  if (stdinWaiter) throw new Error('NppNodeJS protocol request is already pending');
  fs.writeSync(1, payload, null, 'utf8');

  while (true) {
    const newline = syncStdinBuffer.indexOf('\n');
    if (newline >= 0) {
      let line = syncStdinBuffer.slice(0, newline);
      syncStdinBuffer = syncStdinBuffer.slice(newline + 1);
      if (line.endsWith('\r')) line = line.slice(0, -1);
      return line;
    }

    const chunk = Buffer.allocUnsafe(4096);
    const bytesRead = fs.readSync(0, chunk, 0, chunk.length, null);
    if (bytesRead === 0) throw new Error('NppNodeJS closed the protocol input');
    syncStdinBuffer += chunk.toString('utf8', 0, bytesRead);
  }
}

function writeAlert(message, title) {
  const text = String(message ?? '');
  const alertTitle = arguments.length >= 2 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(alertTitle, 'utf8').toString('base64');
  return requestResponse(`${ALERT_PREFIX}${encodedMessage}|${encodedTitle}\n`).then(() => undefined);
}

function prompt(message, defaultValue, title) {
  const text = String(message ?? '');
  const defaultText = arguments.length >= 2 ? String(defaultValue ?? '') : '';
  const promptTitle = arguments.length >= 3 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedDefault = Buffer.from(defaultText, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(promptTitle, 'utf8').toString('base64');

  return requestResponse(`${PROMPT_PREFIX}${encodedMessage}|${encodedDefault}|${encodedTitle}\n`).then((line) => {
    if (line === 'CANCEL') return null;
    if (line.startsWith('OK:')) return Buffer.from(line.slice(3), 'base64').toString('utf8');
    return line;
  });
}

function confirm(message, title) {
  const text = String(message ?? '');
  const confirmTitle = arguments.length >= 2 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(confirmTitle, 'utf8').toString('base64');
  return requestResponse(`${CONFIRM_PREFIX}${encodedMessage}|${encodedTitle}\n`).then((line) => line === 'OK');
}

function getFileName() {
  return requestResponse(`${GETFILE_PREFIX}\n`).then((line) => {
    if (!line.startsWith('OK:')) return '';
    return Buffer.from(line.slice(3), 'base64').toString('utf8');
  });
}

function getCursor() {
  return requestResponse(`${GETCURSOR_PREFIX}\n`).then((line) => {
    if (!line.startsWith('OK:')) return { line: 0, column: 0 };
    const [lineNumber, columnNumber] = line.slice(3).split(',').map(Number);
    return { line: lineNumber, column: columnNumber };
  });
}

function getLine() {
  return requestResponse(`${GETLINE_PREFIX}\n`).then((line) => line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '');
}

function setLine(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponse(`${SETLINE_PREFIX}${encoded}\n`).then((line) => line === 'OK');
}

function getSelection() {
  return requestResponse(`${GETSELECTION_PREFIX}\n`).then((line) => line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '');
}

function setSelection(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponse(`${SETSELECTION_PREFIX}${encoded}\n`).then((line) => line === 'OK');
}

function hasSelection() {
  return requestResponse(`${HASSELECTION_PREFIX}\n`).then((line) => line === 'OK:1');
}

function getText() {
  return requestResponse(`${GETTEXT_PREFIX}\n`).then((line) => line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '');
}

function setText(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponse(`${SETTEXT_PREFIX}${encoded}\n`).then((line) => line === 'OK');
}

function syncAlert(message, title) {
  const text = String(message ?? '');
  const alertTitle = arguments.length >= 2 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(alertTitle, 'utf8').toString('base64');
  requestResponseSync(`${ALERT_PREFIX}${encodedMessage}|${encodedTitle}\n`);
}

function syncPrompt(message, defaultValue, title) {
  const text = String(message ?? '');
  const defaultText = arguments.length >= 2 ? String(defaultValue ?? '') : '';
  const promptTitle = arguments.length >= 3 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedDefault = Buffer.from(defaultText, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(promptTitle, 'utf8').toString('base64');
  const line = requestResponseSync(`${PROMPT_PREFIX}${encodedMessage}|${encodedDefault}|${encodedTitle}\n`);
  if (line === 'CANCEL') return null;
  if (line.startsWith('OK:')) return Buffer.from(line.slice(3), 'base64').toString('utf8');
  return line;
}

function syncConfirm(message, title) {
  const text = String(message ?? '');
  const confirmTitle = arguments.length >= 2 ? String(title ?? '') : getDefaultAlertTitle();
  const encodedMessage = Buffer.from(text, 'utf8').toString('base64');
  const encodedTitle = Buffer.from(confirmTitle, 'utf8').toString('base64');
  return requestResponseSync(`${CONFIRM_PREFIX}${encodedMessage}|${encodedTitle}\n`) === 'OK';
}

function syncGetFileName() {
  const line = requestResponseSync(`${GETFILE_PREFIX}\n`);
  return line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '';
}

function syncGetCursor() {
  const line = requestResponseSync(`${GETCURSOR_PREFIX}\n`);
  if (!line.startsWith('OK:')) return { line: 0, column: 0 };
  const [lineNumber, columnNumber] = line.slice(3).split(',').map(Number);
  return { line: lineNumber, column: columnNumber };
}

function syncGetLine() {
  const line = requestResponseSync(`${GETLINE_PREFIX}\n`);
  return line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '';
}

function syncSetLine(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponseSync(`${SETLINE_PREFIX}${encoded}\n`) === 'OK';
}

function syncGetSelection() {
  const line = requestResponseSync(`${GETSELECTION_PREFIX}\n`);
  return line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '';
}

function syncSetSelection(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponseSync(`${SETSELECTION_PREFIX}${encoded}\n`) === 'OK';
}

function syncHasSelection() {
  return requestResponseSync(`${HASSELECTION_PREFIX}\n`) === 'OK:1';
}

function syncGetText() {
  const line = requestResponseSync(`${GETTEXT_PREFIX}\n`);
  return line.startsWith('OK:') ? Buffer.from(line.slice(3), 'base64').toString('utf8') : '';
}

function syncSetText(text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  return requestResponseSync(`${SETTEXT_PREFIX}${encoded}\n`) === 'OK';
}

module.exports = {
  alert: syncAlert,
  confirm: syncConfirm,
  prompt: syncPrompt,
  getFileName: syncGetFileName,
  getCursor: syncGetCursor,
  getLine: syncGetLine,
  setLine: syncSetLine,
  getSelection: syncGetSelection,
  setSelection: syncSetSelection,
  hasSelection: syncHasSelection,
  getText: syncGetText,
  setText: syncSetText,
  promises: {
    alert: writeAlert,
    confirm,
    prompt,
    getFileName,
    getCursor,
    getLine,
    setLine,
    getSelection,
    setSelection,
    hasSelection,
    getText,
    setText
  }
};
