#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#define TCS_MOD_API_VERSION 1u
#ifdef __cplusplus
#define TCS_MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define TCS_MOD_EXPORT __declspec(dllexport)
#endif

typedef enum TCSSettingType { TCS_SETTING_BOOL=1, TCS_SETTING_INT=2, TCS_SETTING_FLOAT=3, TCS_SETTING_KEY=4, TCS_SETTING_ENUM=5 } TCSSettingType;
typedef struct TCSSettingValue { int integer; float real; } TCSSettingValue;
typedef void (__cdecl *TCSSettingChanged)(TCSSettingValue value, void* context);
typedef struct TCSSettingDescriptor {
    unsigned int structSize;
    const char* category;
    const char* key;
    const char* label;
    TCSSettingType type;
    TCSSettingValue defaultValue;
    float minimum, maximum, step;
    TCSSettingChanged changed;
    void* context;
    const char* const* choices;
    unsigned int choiceCount;
} TCSSettingDescriptor;

typedef struct TCSModInfo { unsigned int structSize; const char* id; const char* name; const char* version; } TCSModInfo;
typedef struct TCSModHost TCSModHost;
typedef void* TCSModHandle;
typedef void (__cdecl *TCSDeviceCreated)(HWND, IDirect3DDevice9*, void*);
typedef void (__cdecl *TCSDeviceReset)(IDirect3DDevice9*, int, void*);
typedef void (__cdecl *TCSEndScene)(IDirect3DDevice9*, void*);
typedef LRESULT (__cdecl *TCSWindowMessage)(HWND, UINT, WPARAM, LPARAM, BOOL*, void*);

struct TCSModHost {
    unsigned int apiVersion, structSize;
    TCSModHandle (__cdecl *registerMod)(const TCSModInfo*);
    BOOL (__cdecl *registerSetting)(TCSModHandle, const TCSSettingDescriptor*);
    void (__cdecl *registerDeviceCallbacks)(TCSModHandle, TCSDeviceCreated, TCSDeviceReset, TCSEndScene, void*);
    void (__cdecl *registerWindowCallback)(TCSModHandle, TCSWindowMessage, void*);
    BOOL (__cdecl *configPath)(TCSModHandle, wchar_t*, unsigned int);
    void (__cdecl *log)(TCSModHandle, const char*);
    BOOL (__cdecl *setVideoMode)(int, int, int, int); // width, height, mode (0 windowed, 1 borderless, 2 exclusive), vsync
    void (__cdecl *setFrameCap)(int);
};

typedef BOOL (__cdecl *TCSModInitialize)(const TCSModHost*, TCSModHandle*);
typedef void (__cdecl *TCSModShutdown)(TCSModHandle);
