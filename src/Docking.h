#pragma once
#include <windows.h>

#define CONT_LEFT 0
#define CONT_RIGHT 1
#define CONT_TOP 2
#define CONT_BOTTOM 3
#define DOCKCONT_MAX 4

#define DWS_ICONTAB 0x00000001
#define DWS_ICONBAR 0x00000002
#define DWS_ADDINFO 0x00000004
#define DWS_USEOWNDARKMODE 0x00000008
#define DWS_PARAMSALL (DWS_ICONTAB|DWS_ICONBAR|DWS_ADDINFO)
#define DWS_DF_CONT_LEFT (CONT_LEFT << 28)
#define DWS_DF_CONT_RIGHT (CONT_RIGHT << 28)
#define DWS_DF_CONT_TOP (CONT_TOP << 28)
#define DWS_DF_CONT_BOTTOM (CONT_BOTTOM << 28)
#define DWS_DF_FLOATING 0x80000000

typedef struct DockedWidgetData {
    HWND hClient = nullptr;
    const wchar_t* pszName = nullptr;
    int dlgID = 0;
    UINT uMask = 0;
    HICON hIconTab = nullptr;
    const wchar_t* pszAddInfo = nullptr;
    RECT rcFloat = {};
    int iPrevCont = 0;
    const wchar_t* pszModuleName = nullptr;
} DockedWidgetData, tTbData;
