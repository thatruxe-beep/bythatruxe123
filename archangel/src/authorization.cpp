// "Authorization" dialog on injection - reconstruction of the original
// FUN_10002c00 / FUN_10002dd0 from DLL.dll.c:
// the original builds an in-memory dialog template (DialogBoxIndirectParamA):
// title "Authorization", static message (ID 1000), label (ID 10),
// EDIT (ID 1001) pre-filled with the saved key, OK/Cancel buttons.
// OK    -> key is read from the EDIT, dialog returns 1
// Cancel-> key is cleared,        dialog returns 2
#include "archangel.h"
#include <stdio.h>
#include <string.h>

#define ID_MSG      1000
#define ID_LABEL    10
#define ID_KEY_EDIT 1001

typedef struct AuthCtx {
    char*     key;
    int       keySize;
    const char* msg;
} AuthCtx;

static INT_PTR CALLBACK AuthDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    AuthCtx* ctx = (AuthCtx*)GetWindowLongA(hDlg, GWLP_USERDATA);
    switch (msg) {
    case WM_INITDIALOG:
        ctx = (AuthCtx*)lParam;
        SetWindowLongA(hDlg, GWLP_USERDATA, (LONG)ctx);
        if (ctx) {
            SetDlgItemTextA(hDlg, ID_MSG, ctx->msg);
            SetDlgItemTextA(hDlg, ID_KEY_EDIT, ctx->key);
            SetFocus(GetDlgItem(hDlg, ID_KEY_EDIT));
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            if (ctx && ctx->key) {
                int n = GetDlgItemTextA(hDlg, ID_KEY_EDIT, ctx->key, ctx->keySize);
                while (n > 0 && (ctx->key[n-1] == ' ' || ctx->key[n-1] == '\t')) ctx->key[--n] = 0;
            }
            EndDialog(hDlg, 1);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, 2);
            return TRUE;
        }
        return FALSE;
    case WM_CLOSE:
        EndDialog(hDlg, 2);
        return TRUE;
    }
    return FALSE;
}

static void P_W2(BYTE** c, const char* s) {
    int len = (int)strlen(s);
    for (int i = 0; i < len; i++) *(*c)++ = (BYTE)(s[i]);
    *(*c)++ = 0; *(*c)++ = 0;
}
static void P_W(BYTE** c, WORD v)   { *(*c)++ = (BYTE)(v & 0xff); *(*c)++ = (BYTE)(v >> 8); }
static void P_D(BYTE** c, DWORD v)  { P_W(c, (WORD)(v & 0xffff)); P_W(c, (WORD)(v >> 16)); }

static DWORD Template_Build(BYTE* out, int outsz) {
    BYTE* c = out;
    // dialog: DS_MODAL|DS_SETFONT|DS_CENTER, WS_SYSMENU
    P_D(&c, 0x000000C4);  P_D(&c, 0x00008000);  // style, exstyle(WS_SYSMENU)
    P_W(&c, 40);  P_W(&c, 250);  P_W(&c, 280);  P_W(&c, 120);  // x y w h (dialog units)
    P_W(&c, 0);                      // menu
    P_W(&c, 14385);                  // class: "DIALOG"
    P_W2(&c, "Authorization");      // title
    P_W(&c, 8); P_W(&c, 0); P_W(&c, 1);P_W(&c, 0);
    P_W2(&c, "MS Sans Serif");
    // controls
    // 1) static message line (ID 1000) - text set at WM_INITDIALOG
    P_D(&c, 0); P_D(&c, 0);
    P_W(&c, 8); P_W(&c, 8); P_W(&c, 260); P_W(&c, 12);
    P_W(&c, ID_MSG);
    P_W2(&c, "static");
    // 2) static label (ID 10)
    P_D(&c, 0); P_D(&c, 0);
    P_W(&c, 8); P_W(&c, 26); P_W(&c, 260); P_W(&c, 12);
    P_W(&c, ID_LABEL);
    P_W2(&c, "static");
    // 3) edit (ID 1001)
    P_D(&c, 0x0011); P_D(&c, 0);     // ES_LEFT | ES_AUTOHSCROLL
    P_W(&c, 8); P_W(&c, 44); P_W(&c, 260); P_W(&c, 20);
    P_W(&c, ID_KEY_EDIT);
    P_W2(&c, "edit");
    // 4) OK
    P_D(&c, 0x0001); P_D(&c, 0);     // BS_DEFPUSHBUTTON
    P_W(&c, 70); P_W(&c, 76); P_W(&c, 70); P_W(&c, 16);
    P_W(&c, IDOK);
    P_W2(&c, "OK");
    // 5) Cancel
    P_D(&c, 0); P_D(&c, 0);
    P_W(&c, 150); P_W(&c, 76); P_W(&c, 70); P_W(&c, 16);
    P_W(&c, IDCANCEL);
    P_W2(&c, "Cancel");
    return (DWORD)(c - out);
}

// returns: 1 = OK, 2 = Cancel
int ShowAuthorizationDialog(char* key, int keySize, const char* msg) {
    static BYTE tmpl[1024];
    Template_Build(tmpl, sizeof(tmpl));
    AuthCtx ctx;
    ctx.key = key;
    ctx.keySize = keySize;
    ctx.msg = msg;
    return (int)DialogBoxIndirectParamA(GetModuleHandleA(NULL), (LPCDLGTEMPLATE)tmpl,
                                        NULL, AuthDlgProc, (LPARAM)&ctx);
}
