// Exercise the actual loader menu state machine with a synthetic native menu.
#include "../src/loader.cpp"

int main(){
    wchar_t folder[MAX_PATH]{},config[MAX_PATH]{};
    GetTempPathW(MAX_PATH,folder);
    if(!GetTempFileNameW(folder,L"tcs",0,config))return 1;
    auto cleanup=[&]{plugins.clear();DeleteFileW(config);};
    int changes=0;
    for(int i=0;i<2;++i){
        auto plugin=std::make_unique<Plugin>();
        plugin->name=i?"Second mod":"First mod";plugin->id=i?"second":"first";plugin->config=config;
        Setting setting;setting.owner=plugin.get();setting.label=i?"Second option":"First option";
        setting.key=i?"Second":"First";setting.type=TCS_SETTING_BOOL;setting.hi=1;
        setting.context=&changes;setting.changed=+[](TCSSettingValue,void* p){++*static_cast<int*>(p);};
        plugin->settings.push_back(setting);plugins.push_back(std::move(plugin));
    }
    alignas(4) unsigned char menu[0xe0]{};
    menuField<short>(menu,0x12)=8;
    openModSettings(menu);
    auto rootRows=nativeRows();
    if(rootRows.size()!=3||rootRows[0].label!=L"First mod"||rootRows[1].label!=L"Second mod"){cleanup();return 2;}
    // Selecting the mod itself opens its controls, without a tab/title button.
    selectNativeRow(menu,0);menuField<int>(menu,0x64)=1;updateModSettings(menu);
    if(selectedPlugin!=plugins[0].get()||nativeRows()[0].label!=L"First option: Off"){cleanup();return 3;}
    menuField<int>(menu,0x64)=1;updateModSettings(menu);
    if(changes!=1||plugins[0]->settings[0].value.integer!=1||plugins[1]->settings[0].value.integer!=0||
       GetPrivateProfileIntW(L"Settings",L"First",0,config)!=1){cleanup();return 4;}
    // First Escape returns to the mod list. Second restores the pause selection.
    menuField<int>(menu,0x68)=1;updateModSettings(menu);
    if(selectedPlugin||!menuOpen){cleanup();return 5;}
    menuField<int>(menu,0x68)=1;updateModSettings(menu);
    if(menuOpen||menuField<short>(menu,0x12)!=8||menuField<short>(menu,0x0a)!=tcs::modSettingsRow){cleanup();return 6;}
    cleanup();return 0;
}
