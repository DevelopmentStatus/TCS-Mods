#pragma once
#include <windows.h>

// Keep the game window's owning thread responsive while its importer runs.
inline DWORD WaitForAssetStartup(HANDLE process, HANDLE reviewing, DWORD timeoutMs, HWND progress=nullptr) {
    ULONGLONG previous=GetTickCount64(),working=0;
    for(;;) {
        DWORD result=MsgWaitForMultipleObjects(1,&process,FALSE,100,QS_ALLINPUT);
        if(result==WAIT_OBJECT_0 || result==WAIT_FAILED)return result;
        ULONGLONG now=GetTickCount64();
        bool paused=reviewing && WaitForSingleObject(reviewing,0)==WAIT_OBJECT_0;
        if(progress)ShowWindow(progress,paused ? SW_HIDE : SW_SHOWNOACTIVATE);
        if(!paused)working+=now-previous;
        previous=now;
        if(working>=timeoutMs)return WAIT_TIMEOUT;
        if(result==WAIT_OBJECT_0+1) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT) {
                    PostQuitMessage(static_cast<int>(message.wParam));
                    return WAIT_ABANDONED;
                }
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }
    }
}
