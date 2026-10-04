#pragma once
#include <windows.h>

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

struct SCNotification {
    NMHDR nmhdr;
};

extern "C" {
    __declspec(dllexport) void setInfo(NppData nppData);
    __declspec(dllexport) const wchar_t* getName();
    struct ShortcutKey { bool _isCtrl; bool _isAlt; bool _isShift; unsigned char _key; };
    struct FuncItem { wchar_t _itemName[64]; void (*_pFunc)(); int _cmdID; bool _init2Check; ShortcutKey* _pShKey; };
    __declspec(dllexport) FuncItem* getFuncsArray(int* nbF);
    __declspec(dllexport) void beNotified(SCNotification* notifyCode);
    __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam);
    __declspec(dllexport) BOOL isUnicode();
}
