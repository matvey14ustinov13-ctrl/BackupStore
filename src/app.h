#pragma once

#include "catalog.h"
#include "common.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

constexpr wchar_t kAppName[] = L"BackupStore";
constexpr wchar_t kAppVersion[] = L"1.0.0";
constexpr wchar_t kAppVersionTag[] = L"v1.0.0";
constexpr wchar_t kStoreRepo[] = L"BackupStore";
constexpr wchar_t kOwner[] = L"matvey14ustinov13-ctrl";

constexpr UINT WM_STORE_STATUS = WM_APP + 10;
constexpr UINT WM_STORE_CATALOG = WM_APP + 11;
constexpr UINT WM_STORE_PROGRESS = WM_APP + 12;
constexpr UINT WM_STORE_OPDONE = WM_APP + 13;
constexpr UINT WM_STORE_STOREUPDATE = WM_APP + 14;

struct StatusPayload {
    std::wstring text;
    bool error = false;
};

struct CatalogPayload {
    bool ok = false;
    bool fromApi = false;
    std::wstring error;
    Catalog catalog;
};

struct ProgressPayload {
    std::wstring repo;
    int percent = 0;
    std::wstring text;
};

struct OpDonePayload {
    std::wstring repo;
    std::wstring version;
    std::wstring exe;
    std::wstring error;
    bool ok = false;
    bool selfUpdate = false;
};

struct StoreUpdatePayload {
    bool ok = false;
    bool manual = false;
    std::wstring error;
    ReleaseInfo release;
};

enum View {
    VIEW_CATALOG = 0,
    VIEW_INSTALLED = 1,
    VIEW_UPDATES = 2,
    VIEW_SETTINGS = 3
};

enum Action {
    ACT_NONE = 0,
    ACT_NAV,
    ACT_REFRESH,
    ACT_INSTALL,
    ACT_LAUNCH,
    ACT_FOLDER,
    ACT_UNINSTALL,
    ACT_RELEASE,
    ACT_SELF_UPDATE,
    ACT_CHECK_STORE_UPDATE,
    ACT_OPEN_APPS,
    ACT_OPEN_DOWNLOADS,
    ACT_GITHUB,
    ACT_SEARCH,
    ACT_LICENSE
};

struct HitTarget {
    RECT rect{};
    Action action = ACT_NONE;
    std::wstring arg;
    int param = 0;
};

struct AppState {
    HWND hwnd = nullptr;
    UINT dpi = 96;

    std::vector<AppInfo> apps;
    Catalog catalog;
    InstalledMap installed;
    std::optional<ReleaseInfo> storeRelease;
    bool storeUpdateAvailable = false;
    bool checkingStoreUpdate = false;

    View view = VIEW_CATALOG;
    bool loadingCatalog = false;
    std::wstring status = L"Готово";
    bool statusError = false;

    std::wstring search;
    bool searchFocused = false;
    bool caretVisible = true;

    int scroll = 0;
    int contentHeight = 0;

    POINT mouse{ -1, -1 };
    bool mouseInside = false;
    std::wstring hoverKey;
    std::vector<HitTarget> targets;

    bool installing = false;
    std::wstring installingRepo;
    int installPercent = 0;
    std::wstring installText;
    bool selfUpdating = false;

    HFONT fontTitle = nullptr;
    HFONT fontHeading = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontBodyBold = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontBadge = nullptr;

    RECT searchRect{};
    RECT contentRect{};

    int D(int value) const {
        return MulDiv(value, static_cast<int>(dpi), 96);
    }
};

void CreateUiFonts(AppState& state);
void DestroyUiFonts(AppState& state);
void PaintUi(AppState& state, HDC dc, const RECT& client);

std::wstring TargetKey(const HitTarget& target);
bool HitTest(const AppState& state, POINT point, HitTarget& target);
const AppInfo* FindStoreApp(const AppState& state, const std::wstring& repo);
int CountUpdates(const AppState& state);
int CountCatalogUpdates(const AppState& state);
std::wstring VersionBadgeText(const AppInfo& app);
bool IsInstalled(const AppState& state, const std::wstring& repo);
