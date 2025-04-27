#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Services.h>
#include <Hatch/Game.h>

#include <assert.h>

#pragma pack(push, 8)
#include "discord_game_sdk.h"
#pragma pack(pop)

#define DISCORD_REQUIRE(x) assert(x == DiscordResult_Ok)

#pragma comment(lib, "discord_game_sdk.dll.lib")

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUserManager* users;
    struct IDiscordAchievementManager* achievements;
    struct IDiscordActivityManager* activities;
    struct IDiscordRelationshipManager* relationships;
    struct IDiscordApplicationManager* application;
    struct IDiscordLobbyManager* lobbies;
    DiscordUserId user_id;
};


namespace Services {
    ServicesFunctionSet Service;

    struct Application app;
    struct IDiscordUserEvents users_events;
    struct IDiscordActivityEvents activities_events;
    struct IDiscordRelationshipEvents relationships_events;
    struct DiscordCreateParams params;

    bool OAuthSuccess = false;
    bool CancelFunctions = false;

    void UpdateActivityCallback(void* data, enum EDiscordResult result) {
        DISCORD_REQUIRE(result);
    }
    void OnUserUpdated(void* data) {
        struct Application* app = (struct Application*)data;
        struct DiscordUser user;
        app->users->get_current_user(app->users, &user);
        app->user_id = user.id;
        // printf("user.avatar: %s\n", user.avatar);
        // printf("user.discriminator: %s\n", user.discriminator);
        // printf("user.username: %s\n", user.username);
    }
    void OnOAuth2Token(void* data, enum EDiscordResult result, struct DiscordOAuth2Token* token) {
        Services::OAuthSuccess = (result == DiscordResult_Ok);
        // printf("OAuth2 token: %s\n", token->access_token);

        if (result != DiscordResult_Ok)
            printf("GetOAuth2Token failed with %d\n", (int)result);
    }

    void Init() {
        // ([A-Za-z0-9_]+) +\(\*([A-Za-z0-9_]+)\)\(([A-Za-z0-9_ \(\)\*,]*)\)
        // $2 = []($3) -> $1 {}

        memset(&app, 0, sizeof(app));
        memset(&users_events, 0, sizeof(users_events));
        memset(&activities_events, 0, sizeof(activities_events));
        memset(&relationships_events, 0, sizeof(relationships_events));

        users_events.on_current_user_update = OnUserUpdated;
        // relationships_events.on_refresh = OnRelationshipsRefresh;

        DiscordCreateParamsSetDefault(&params);
        params.client_id = 0;
        params.flags = DiscordCreateFlags_Default;
        params.event_data = &app;
        params.activity_events = &activities_events;
        params.relationship_events = &relationships_events;
        params.user_events = &users_events;

        EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &app.core);
        DISCORD_REQUIRE(result);

        if (result != EDiscordResult::DiscordResult_Ok) {
            CancelFunctions = true;
        }
        else {
            app.users = app.core->get_user_manager(app.core);
            app.achievements = app.core->get_achievement_manager(app.core);
            app.activities = app.core->get_activity_manager(app.core);
            app.application = app.core->get_application_manager(app.core);
            app.lobbies = app.core->get_lobby_manager(app.core);
            app.relationships = app.core->get_relationship_manager(app.core);

            app.application->get_oauth2_token(app.application, &app, OnOAuth2Token);

            /*
            // Get User Name
            struct Application* app = (struct Application*)data;
            struct DiscordUser user;
            app->users->get_current_user(app->users, &user);
            app->user_id = user.id;
            printf("user.avatar: %s\n", user.avatar);
            printf("user.discriminator: %s\n", user.discriminator);
            printf("user.username: %s\n", user.username);
            //*/
        }

        Service.Core.GetLocale = []() -> int { return 0; };
        Service.Core.GetConfirmButtonFlip = []() -> bool {
            return false;
        };
        Service.Core.IsMobile = []() -> bool {
            return false;
        };
        Service.Core.CanExitGame = []() -> bool {
            return true;
        };
        Service.Core.ExitGame = []() -> void {
            Game::Running = false;
        };
        Service.Core.Run = []() -> void { 
            if (CancelFunctions)
                return;

            // DISCORD_REQUIRE(app.core->run_callbacks(app.core));
            if (app.core->run_callbacks(app.core) != DiscordResult_Ok)
                CancelFunctions = true;
        };
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
        Service.UserData.UpdateRichPresence = [](CString state, CString details, CString image, Sint64 timeStart, Sint64 timeEnd) -> void {
            if (CancelFunctions)
                return;

            struct Application* app = &Services::app;

            struct DiscordActivity activity;
            memset(&activity, 0, sizeof(activity));
            if (details) sprintf(activity.details, details);
            if (state) sprintf(activity.state, state);
            if (image) sprintf(activity.assets.large_image, image);
                
            activity.timestamps.start = timeStart;
            activity.timestamps.end = timeEnd;

            app->activities->update_activity(app->activities, &activity, app, UpdateActivityCallback);
        };
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
            char path[256];
            snprintf(path, 256, "%s", filename);

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
        };
        Service.UserStorage.WriteSaveFile = [](CString filename, void* data, Uint32 dataSize, void (*resolve)(int code), bool compress) -> bool {
            char path[256];
            snprintf(path, 256, "%s", filename);

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
