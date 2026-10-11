#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <MinHook.h>

#include "blredirect/logging.h"

namespace {

namespace fs = std::filesystem;

fs::path plugin_dir;

void setup_logging(HMODULE module) {
    wchar_t module_path[MAX_PATH];
    if (GetModuleFileNameW(module, module_path, MAX_PATH) != 0) {
        plugin_dir = fs::path{module_path}.parent_path();
        blredirect::setup_logging(plugin_dir / "blredirect.log");
    }
    BLREDIRECT_LOG("BLRedirect starting...");
}

}  // namespace

// NOLINTNEXTLINE(readability-identifier-naming,misc-use-internal-linkage)
BOOL APIENTRY DllMain(HMODULE hinst_dll, DWORD fdw_reason, LPVOID /*lpv_reserved*/) {
    switch (fdw_reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hinst_dll);
            setup_logging(hinst_dll);
            MH_Initialize();
            break;
        }

        case DLL_PROCESS_DETACH: {
            BLREDIRECT_LOG("BLRedirect shutting down...");
            MH_Uninitialize();
            blredirect::shutdown_logging();
            break;
        }

        default: {
            break;
        }
    }
    return TRUE;
}
