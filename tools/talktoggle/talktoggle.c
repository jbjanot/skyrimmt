// talktoggle.exe — Keizaal Online's push-to-talk as a toggle: one press of the toggle key holds the talk key down,
// the next press releases it. Runs in the game's Wine prefix next to Keizaal's launcher (scripts/keizaal.sh starts it):
//   talktoggle.exe [TOGGLE TALK]     keys as F1…F12 or a letter; default F3 V (Keizaal binds F1, F2 and F6 already)
// A low-level keyboard hook only sees keys typed into Wine windows, so other Mac apps keep the toggle key, and the
// held key is injected like a real one (DirectInput, raw input and hooks all see it). It swallows the toggle key,
// lets a real press of the talk key end the toggle, and exits (releasing the key) once neither Keizaal's launcher nor
// Skyrim runs. One instance per prefix.
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <ctype.h>

static UINT toggleVk = VK_F3, talkVk = 'V';
static BOOL held;
static DWORD mainThread;
#define WM_FLIP (WM_APP + 1)

static UINT parseKey(const char *s, UINT fallback) {
    int n;
    if ((s[0] == 'F' || s[0] == 'f') && sscanf(s + 1, "%d", &n) == 1 && n >= 1 && n <= 12) return VK_F1 + n - 1;
    if (s[0] && !s[1] && isalnum((unsigned char)s[0])) return (UINT)toupper((unsigned char)s[0]);
    return fallback;
}

static void sendTalk(BOOL down) {
    INPUT in = {0};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = (WORD)talkVk;
    in.ki.wScan = (WORD)MapVirtualKeyA(talkVk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof in);
    held = down;
}

static LRESULT CALLBACK hook(int code, WPARAM wp, LPARAM lp) {
    const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
    if (code == HC_ACTION && !(k->flags & LLKHF_INJECTED)) {
        BOOL down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        if (k->vkCode == toggleVk) {   // swallowed; the flip runs outside the hook (SendInput from here would re-enter it)
            if (down && !(k->flags & LLKHF_UP)) PostThreadMessageA(mainThread, WM_FLIP, 0, 0);
            return 1;
        }
        if (k->vkCode == talkVk && !down) held = FALSE;   // a real press of the talk key took over: its release ends it
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

static BOOL running(const char *const names[], int count) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 p = { .dwSize = sizeof p };
    BOOL found = FALSE;
    for (BOOL ok = Process32First(snap, &p); ok && !found; ok = Process32Next(snap, &p))
        for (int i = 0; i < count; i++) if (!lstrcmpiA(p.szExeFile, names[i])) found = TRUE;
    CloseHandle(snap);
    return found;
}

int main(int argc, char **argv) {
    if (argc > 1) toggleVk = parseKey(argv[1], toggleVk);
    if (argc > 2) talkVk = parseKey(argv[2], talkVk);
    CreateMutexA(NULL, FALSE, "SkyrimMT-talktoggle");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    mainThread = GetCurrentThreadId();
    HHOOK h = SetWindowsHookExA(WH_KEYBOARD_LL, hook, GetModuleHandleA(NULL), 0);
    if (!h) { fprintf(stderr, "talktoggle: no keyboard hook (%lu)\n", GetLastError()); return 1; }
    static const char *const keizaal[] = { "keizaal-online.exe", "SkyrimSE.exe" };
    SetTimer(NULL, 0, 5000, NULL);
    printf("talktoggle: key %#x toggles %#x\n", toggleVk, talkVk); fflush(stdout);
    MSG m;
    int idle = 0;   // timer ticks without Keizaal: started before its launcher, so allow it a minute to appear
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        if (m.message == WM_FLIP) sendTalk(!held);
        else if (m.message == WM_TIMER) idle = running(keizaal, 2) ? -1 : (idle < 0 ? 99 : idle + 1);
        if (idle >= 12) break;
    }
    if (held) sendTalk(FALSE);
    UnhookWindowsHookEx(h);
    return 0;
}
