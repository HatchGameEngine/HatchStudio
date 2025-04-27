#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Services.h>

#include "SDL.h"

namespace Services {
    ServicesFunctionSet Service;

    void Init() {
        // ([A-Za-z0-9_]+) +\(\*([A-Za-z0-9_]+)\)\(([A-Za-z0-9_ \(\)\*,]*)\)
        // $2 = []($3) -> $1 {}

        Service.Core.GetLocale = []() -> int { return 0; };
        Service.Core.GetConfirmButtonFlip = []() -> bool {
            return false;
        };
        Service.Core.IsMobile = []() -> bool {
            return true;
        };
        Service.Core.CanExitGame = []() -> bool {
            return false;
        };
        Service.Core.ExitGame = []() -> void {};
        Service.Core.Run = []() -> void { };
        Service.Core.LaunchManual = []() -> void {};
        Service.Core.GetSafeViewMargins = [](int* x1, int* y1, int* x2, int* y2) -> void {};
        Service.Core.ShowInputDeviceConfigOverlay = [](Uint32 deviceID) -> bool {
            return false;
        };
        Service.Core.IsEntitlementEnabled = [](Uint32 extensionID) -> bool {
            return false;
        };
        Service.Core.ShowEntitlementOverlay = [](Uint32 extensionID) -> bool {
            return false;
        };

        Service.UserData.ShowUserProfileOverlay = [](CString userID, CString username) -> bool {
            return false;
        };
        Service.UserData.UnlockAchievement = [](void* infoStructPtr) -> void {};
        Service.UserData.GetAchievementsEnabled = []() -> bool {
            return false;
        };
        Service.UserData.SetAchievementsEnabled = [](bool enabled) -> void {};
        Service.UserData.UpdateRichPresence = [](CString state, CString details, CString image, Sint64 start, Sint64 end) -> void {};
        Service.UserData.ClearRichPresence = []() -> void {};

        Service.UserStorage.TryInitStorage = []() -> bool {
            return false;
        };
        Service.UserStorage.GetStorageStatus = []() -> int {
            return 0;
        };
        Service.UserStorage.GetStoragePermission = []() -> int {
            return 0;
        };
        Service.UserStorage.StoragePermissionReset = []() -> void {};
        Service.UserStorage.StoragePermissionRequestBegin = []() -> void {};
        Service.UserStorage.StoragePermissionRequestGrant = []() -> void {};
        Service.UserStorage.StoragePermissionRequestDeny = []() -> void {};
        Service.UserStorage.StoragePermissionRequestErrorOut = []() -> void {};
        Service.UserStorage.NoSaveModeEnable = [](bool noSave) -> void {};
        Service.UserStorage.IsNoSaveModeEnabled = []() -> bool { return false; };
        Service.UserStorage.ReadSaveFile = [](CString filename, void* data, Uint32 dataSize, void (*resolve)(int code)) -> bool {
            char* base_path = SDL_GetPrefPath(ORGANIZATION_NAME, APPLICATION_NAME);
            if (base_path) {
                char path[256];
                snprintf(path, 256, "%s%s", base_path, filename);
                SDL_free(base_path);

                FILE* f = fopen(path, "rb");
                if (f) {
                    fread(data, dataSize, 1, f);
                    fclose(f);
                    resolve(STATUS_OK);
                }
                else {
                    resolve(STATUS_ERR);
                }
                return false;
            }
            resolve(STATUS_ERR);
            return false;
        };
        Service.UserStorage.WriteSaveFile = [](CString filename, void* data, Uint32 dataSize, void (*resolve)(int code), bool compress) -> bool {
            char* base_path = SDL_GetPrefPath(ORGANIZATION_NAME, APPLICATION_NAME);
            if (base_path) {
                char path[256];
                snprintf(path, 256, "%s%s", base_path, filename);
                SDL_free(base_path);

                FILE* f = fopen(path, "wb");
                if (f) {
                    fwrite(data, dataSize, 1, f);
                    fclose(f);
                    resolve(STATUS_OK);
                }
                else {
                    resolve(STATUS_ERR);
                }
                return false;
            }
            resolve(STATUS_ERR);
            return false;
        };
        Service.UserStorage.DeleteSaveFile = [](CString filename, void (*resolve)(int code)) -> bool {
            return false;
        };

        Service.WifiP2P.IsSupported = []() -> int {
            return 0;
        };
        Service.WifiP2P.Init = []() -> int {
            return 0;
        };
        Service.WifiP2P.DiscoverPeers = [](void (*onSuccess)(), void (*onFailure)(int errorCode)) -> int {
            return 0;
        };
        Service.WifiP2P.RequestPeers = [](void (*onSuccess)()) -> int {
            return 0;
        };
        Service.WifiP2P.ConnectToPeer = [](void (*onSuccess)()) -> int {
            return 0;
        };
    }
    void Dispose() {

    }
}
