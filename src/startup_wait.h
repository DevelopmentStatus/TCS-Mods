#pragma once
#include <windows.h>

// Direct3DCreate9 has not returned yet. Dispatch only the loader's status
// window: consuming the game's queued activation/input messages here can run
// its window procedure against an unfinished graphics/startup state.
inline DWORD WaitForAssetStartup(HANDLE process, HANDLE reviewing, DWORD timeoutMs, HWND progress=nullptr) {
    ULONGLONG previous=GetTickCount64(),working=0;
    for(;;) {
        DWORD result=WaitForSingleObject(process,50);
        if(result==WAIT_OBJECT_0 || result==WAIT_FAILED)return result;
        ULONGLONG now=GetTickCount64();
        bool paused=reviewing && WaitForSingleObject(reviewing,0)==WAIT_OBJECT_0;
        if(progress)ShowWindow(progress,paused ? SW_HIDE : SW_SHOWNOACTIVATE);
        if(!paused)working+=now-previous;
        previous=now;
        if(working>=timeoutMs)return WAIT_TIMEOUT;
        MSG message{};
        if(PeekMessageW(&message,nullptr,WM_QUIT,WM_QUIT,PM_NOREMOVE))return WAIT_ABANDONED;
        if(progress && IsWindow(progress)) {
            while(PeekMessageW(&message,progress,0,0,PM_REMOVE)) {
                // WM_QUIT ignores the HWND filter; leave it for the game.
                if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return WAIT_ABANDONED;}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }
    }
}
