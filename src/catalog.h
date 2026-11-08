#pragma once

#include "common.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

extern const wchar_t* const kOwnerName;
extern const wchar_t* const kStoreRepoName;

struct AssetInfo {
    std::wstring name;
    std::wstring url;
    std::wstring contentType;
    long long size = 0;
};

struct ReleaseInfo {
    std::wstring tag;
    std::wstring title;
    std::wstring notes;
    std::wstring publishedAt;
    std::wstring htmlUrl;
    bool prerelease = false;
    std::vector<AssetInfo> assets;
};

struct AppInfo {
    std::wstring repo;
    std::wstring fullName;
    std::wstring description;
    std::wstring htmlUrl;
    std::wstring license;
    std::wstring updatedAt;
    std::vector<std::wstring> topics;
    int stars = 0;
    bool archived = false;
    bool hasRelease = false;
    ReleaseInfo release;
};

struct Catalog {
    std::wstring generatedAt;
    std::vector<AppInfo> apps;
};

struct InstalledApp {
    std::wstring repo;
    std::wstring version;
    std::wstring path;
    std::wstring exe;
    std::wstring installedAt;
    std::wstring title;
};

using InstalledMap = std::map<std::wstring, InstalledApp>;
using StatusCallback = std::function<void(const std::wstring&)>;

std::wstring CatalogRawUrl();

bool ParseCatalogJson(const std::string& json, Catalog& catalog, std::wstring& error);
std::optional<Catalog> LoadCachedCatalog(std::wstring& error);
bool SaveCachedCatalog(const Catalog& catalog, const std::string& rawJson);
std::string SerializeCatalog(const Catalog& catalog);

std::optional<Catalog> FetchCatalogFromRaw(const StatusCallback& status, std::wstring& error);
std::optional<Catalog> FetchCatalogFromApi(const StatusCallback& status, std::wstring& error);
std::optional<ReleaseInfo> FetchLatestRelease(const std::wstring& owner,
                                              const std::wstring& repo,
                                              std::wstring& error);

InstalledMap LoadInstalled();
bool SaveInstalled(const InstalledMap& installed);

const AppInfo* FindApp(const std::vector<AppInfo>& apps, const std::wstring& repo);
const AssetInfo* ChooseAsset(const ReleaseInfo& release, bool preferExe = false);
bool HasUpdate(const InstalledApp& installed, const AppInfo& app);
