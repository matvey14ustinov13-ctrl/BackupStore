#include "catalog.h"

#include "http.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>

using nlohmann::json;

const wchar_t* const kOwnerName = L"matvey14ustinov13-ctrl";
const wchar_t* const kStoreRepoName = L"BackupStore";

namespace {

std::wstring JsonString(const json& object, const char* key) {
    auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return std::wstring();
    }
    if (it->is_string()) {
        return util::Utf8ToWide(it->get<std::string>());
    }
    return std::wstring();
}

long long JsonInt(const json& object, const char* key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return 0;
    }
    return it->get<long long>();
}

bool ParseRelease(const json& value, ReleaseInfo& release) {
    if (!value.is_object()) {
        return false;
    }
    release.tag = JsonString(value, "tag_name");
    release.title = JsonString(value, "name");
    if (release.title.empty()) {
        release.title = release.tag;
    }
    release.notes = JsonString(value, "body");
    release.htmlUrl = JsonString(value, "html_url");
    release.publishedAt = JsonString(value, "published_at");
    release.prerelease = value.value("prerelease", false);

    auto assets = value.find("assets");
    if (assets != value.end() && assets->is_array()) {
        for (const auto& item : *assets) {
            AssetInfo asset;
            asset.name = JsonString(item, "name");
            asset.url = JsonString(item, "browser_download_url");
            asset.contentType = JsonString(item, "content_type");
            asset.size = JsonInt(item, "size");
            if (!asset.name.empty() && !asset.url.empty()) {
                release.assets.push_back(std::move(asset));
            }
        }
    }
    return !release.tag.empty();
}

bool ParseApp(const json& value, AppInfo& app) {
    if (!value.is_object()) {
        return false;
    }
    app.repo = JsonString(value, "name");
    app.fullName = JsonString(value, "full_name");
    app.description = JsonString(value, "description");
    app.htmlUrl = JsonString(value, "html_url");
    app.updatedAt = JsonString(value, "updated_at");
    app.stars = static_cast<int>(JsonInt(value, "stargazers_count"));
    app.archived = value.value("archived", false);

    auto license = value.find("license");
    if (license != value.end() && license->is_object()) {
        app.license = JsonString(*license, "spdx_id");
    }
    auto topics = value.find("topics");
    if (topics != value.end() && topics->is_array()) {
        for (const auto& topic : *topics) {
            if (topic.is_string()) {
                app.topics.push_back(util::Utf8ToWide(topic.get<std::string>()));
            }
        }
    }

    auto release = value.find("latest_release");
    if (release == value.end()) {
        release = value.find("latest");
    }
    if (release != value.end() && release->is_object()) {
        app.hasRelease = ParseRelease(*release, app.release);
        if (app.release.title.empty()) {
            app.release.title = app.release.tag;
        }
    }
    return !app.repo.empty();
}

std::wstring BuildApiUrl(const std::wstring& path) {
    return L"https://api.github.com" + path;
}

std::vector<std::wstring> GitHubHeaders() {
    return {
        L"User-Agent: BackupStore/1.0",
        L"Accept: application/vnd.github+json",
        L"X-GitHub-Api-Version: 2022-11-28"
    };
}

std::optional<json> GetJson(const std::wstring& url, std::wstring& error) {
    HttpResult response = HttpGet(url, GitHubHeaders());
    if (!response.ok) {
        error = response.error;
        return std::nullopt;
    }
    try {
        return json::parse(response.body);
    } catch (const std::exception& ex) {
        error = L"Некорректный ответ GitHub: " + util::Utf8ToWide(ex.what());
        return std::nullopt;
    }
}

std::optional<json> SelectLatestRelease(const json& releases) {
    if (!releases.is_array() || releases.empty()) {
        return std::nullopt;
    }
    const json* fallback = nullptr;
    for (const auto& release : releases) {
        if (!release.is_object() || release.value("draft", false)) {
            continue;
        }
        if (!release.value("prerelease", false)) {
            return release;
        }
        if (!fallback) {
            fallback = &release;
        }
    }
    if (fallback) {
        return *fallback;
    }
    return std::nullopt;
}

} // namespace

std::wstring CatalogRawUrl() {
    return std::wstring(L"https://raw.githubusercontent.com/") + kOwnerName + L"/" + kStoreRepoName +
           L"/main/catalog.json";
}

bool ParseCatalogJson(const std::string& jsonText, Catalog& catalog, std::wstring& error) {
    try {
        json root = json::parse(jsonText);
        if (!root.is_object()) {
            error = L"Каталог имеет неверный формат";
            return false;
        }
        catalog = Catalog();
        catalog.generatedAt = JsonString(root, "generated_at");

        auto apps = root.find("apps");
        if (apps == root.end() || !apps->is_array()) {
            error = L"В каталоге нет списка приложений";
            return false;
        }
        for (const auto& item : *apps) {
            AppInfo app;
            if (ParseApp(item, app) && app.hasRelease) {
                catalog.apps.push_back(std::move(app));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        error = L"Ошибка разбора каталога: " + util::Utf8ToWide(ex.what());
        return false;
    }
}

std::optional<Catalog> LoadCachedCatalog(std::wstring& error) {
    std::wstring path = util::CatalogCachePath();
    if (!util::FileExists(path)) {
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = L"Не удалось открыть кэш каталога";
        return std::nullopt;
    }
    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Catalog catalog;
    if (!ParseCatalogJson(text, catalog, error)) {
        return std::nullopt;
    }
    return catalog;
}

bool SaveCachedCatalog(const Catalog& catalog, const std::string& rawJson) {
    if (!util::EnsureDir(util::CacheDir())) {
        return false;
    }
    std::string text = rawJson;
    if (text.empty()) {
        text = SerializeCatalog(catalog);
    }
    std::ofstream file(util::CatalogCachePath(), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

std::string SerializeCatalog(const Catalog& catalog) {
    json root;
    root["generated_at"] = util::WideToUtf8(catalog.generatedAt);
    json apps = json::array();
    for (const AppInfo& app : catalog.apps) {
        json item;
        item["name"] = util::WideToUtf8(app.repo);
        item["full_name"] = util::WideToUtf8(app.fullName);
        item["description"] = util::WideToUtf8(app.description);
        item["html_url"] = util::WideToUtf8(app.htmlUrl);
        item["updated_at"] = util::WideToUtf8(app.updatedAt);
        item["stargazers_count"] = app.stars;
        item["archived"] = app.archived;
        if (!app.license.empty()) {
            item["license"] = { { "spdx_id", util::WideToUtf8(app.license) } };
        }
        json topics = json::array();
        for (const auto& topic : app.topics) {
            topics.push_back(util::WideToUtf8(topic));
        }
        item["topics"] = topics;

        json release;
        release["tag_name"] = util::WideToUtf8(app.release.tag);
        release["name"] = util::WideToUtf8(app.release.title);
        release["body"] = util::WideToUtf8(app.release.notes);
        release["published_at"] = util::WideToUtf8(app.release.publishedAt);
        release["html_url"] = util::WideToUtf8(app.release.htmlUrl);
        release["prerelease"] = app.release.prerelease;
        json assets = json::array();
        for (const AssetInfo& asset : app.release.assets) {
            json a;
            a["name"] = util::WideToUtf8(asset.name);
            a["browser_download_url"] = util::WideToUtf8(asset.url);
            a["content_type"] = util::WideToUtf8(asset.contentType);
            a["size"] = asset.size;
            assets.push_back(std::move(a));
        }
        release["assets"] = assets;
        item["latest_release"] = release;
        apps.push_back(std::move(item));
    }
    root["apps"] = apps;
    return root.dump(2);
}

std::optional<Catalog> FetchCatalogFromRaw(const StatusCallback& status, std::wstring& error) {
    if (status) {
        status(L"Загрузка каталога…");
    }
    HttpResult response = HttpGet(CatalogRawUrl(), { L"User-Agent: BackupStore/1.0" });
    if (!response.ok) {
        error = response.error;
        return std::nullopt;
    }
    Catalog catalog;
    if (!ParseCatalogJson(response.body, catalog, error)) {
        return std::nullopt;
    }
    if (catalog.generatedAt.empty()) {
        catalog.generatedAt = util::NowIso8601();
    }
    SaveCachedCatalog(catalog, response.body);
    return catalog;
}

std::optional<Catalog> FetchCatalogFromApi(const StatusCallback& status, std::wstring& error) {
    if (status) {
        status(L"Получение списка репозиториев…");
    }

    Catalog catalog;
    catalog.generatedAt = util::NowIso8601();

    std::vector<json> repositories;
    for (int page = 1; page <= 5; ++page) {
        std::wstring url = BuildApiUrl(L"/users/" + std::wstring(kOwnerName) +
                                       L"/repos?per_page=100&sort=updated&type=owner&page=" +
                                       std::to_wstring(page));
        std::wstring pageError;
        auto list = GetJson(url, pageError);
        if (!list) {
            error = pageError;
            return std::nullopt;
        }
        if (!list->is_array()) {
            error = L"GitHub вернул неожиданный список репозиториев";
            return std::nullopt;
        }
        if (list->empty()) {
            break;
        }
        for (const auto& repo : *list) {
            repositories.push_back(repo);
        }
        if (list->size() < 100) {
            break;
        }
    }

    for (const auto& repo : repositories) {
        if (!repo.is_object()) {
            continue;
        }
        if (repo.value("fork", false)) {
            continue;
        }
        std::wstring name = JsonString(repo, "name");
        if (name.empty() || _wcsicmp(name.c_str(), kStoreRepoName) == 0) {
            continue;
        }

        if (status) {
            status(L"Проверка релизов: " + name);
        }
        std::wstring releasesUrl = BuildApiUrl(L"/repos/" + std::wstring(kOwnerName) + L"/" + name +
                                               L"/releases?per_page=10");
        std::wstring releaseError;
        auto releases = GetJson(releasesUrl, releaseError);
        if (!releases) {
            // Репозиторий без релизов или ошибка доступа — пропускаем.
            continue;
        }
        auto latest = SelectLatestRelease(*releases);
        if (!latest) {
            continue;
        }

        AppInfo app;
        if (!ParseApp(repo, app)) {
            continue;
        }
        if (!ParseRelease(*latest, app.release)) {
            continue;
        }
        app.hasRelease = true;

        // У repos API нет поля latest_release, но ParseApp ожидает его.
        if (app.release.title.empty()) {
            app.release.title = app.release.tag;
        }
        catalog.apps.push_back(std::move(app));
    }

    if (catalog.apps.empty()) {
        error = L"Не найдено ни одного приложения с релизами";
        return std::nullopt;
    }

    Catalog normalized = catalog;
    SaveCachedCatalog(normalized, std::string());
    return catalog;
}

std::optional<ReleaseInfo> FetchLatestRelease(const std::wstring& owner,
                                              const std::wstring& repo,
                                              std::wstring& error) {
    std::wstring url = BuildApiUrl(L"/repos/" + owner + L"/" + repo + L"/releases?per_page=10");
    auto releases = GetJson(url, error);
    if (!releases) {
        return std::nullopt;
    }
    auto latest = SelectLatestRelease(*releases);
    if (!latest) {
        error = L"Релизы не найдены";
        return std::nullopt;
    }
    ReleaseInfo release;
    if (!ParseRelease(*latest, release)) {
        error = L"Не удалось разобрать релиз";
        return std::nullopt;
    }
    return release;
}

InstalledMap LoadInstalled() {
    InstalledMap result;
    std::wstring path = util::InstalledFilePath();
    if (!util::FileExists(path)) {
        return result;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return result;
    }
    try {
        json root;
        file >> root;
        auto apps = root.find("apps");
        if (apps == root.end() || !apps->is_array()) {
            return result;
        }
        for (const auto& item : *apps) {
            InstalledApp app;
            app.repo = JsonString(item, "repo");
            app.version = JsonString(item, "version");
            app.path = JsonString(item, "path");
            app.exe = JsonString(item, "exe");
            app.installedAt = JsonString(item, "installed_at");
            app.title = JsonString(item, "title");
            if (!app.repo.empty()) {
                result[app.repo] = std::move(app);
            }
        }
    } catch (...) {
        result.clear();
    }
    return result;
}

bool SaveInstalled(const InstalledMap& installed) {
    if (!util::EnsureDir(util::DataDir())) {
        return false;
    }
    json root;
    json apps = json::array();
    for (const auto& [repo, app] : installed) {
        (void)repo;
        json item;
        item["repo"] = util::WideToUtf8(app.repo);
        item["version"] = util::WideToUtf8(app.version);
        item["path"] = util::WideToUtf8(app.path);
        item["exe"] = util::WideToUtf8(app.exe);
        item["installed_at"] = util::WideToUtf8(app.installedAt);
        item["title"] = util::WideToUtf8(app.title);
        apps.push_back(std::move(item));
    }
    root["apps"] = apps;
    std::ofstream file(util::InstalledFilePath(), std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << root.dump(2);
    return static_cast<bool>(file);
}

const AppInfo* FindApp(const std::vector<AppInfo>& apps, const std::wstring& repo) {
    for (const AppInfo& app : apps) {
        if (_wcsicmp(app.repo.c_str(), repo.c_str()) == 0) {
            return &app;
        }
    }
    return nullptr;
}

const AssetInfo* ChooseAsset(const ReleaseInfo& release, bool preferExe) {
    const AssetInfo* zip = nullptr;
    const AssetInfo* exe = nullptr;
    for (const AssetInfo& asset : release.assets) {
        std::wstring lower = util::ToLower(asset.name);
        bool isExe = lower.size() > 4 && lower.substr(lower.size() - 4) == L".exe";
        bool isZip = lower.size() > 4 && lower.substr(lower.size() - 4) == L".zip";
        if (preferExe && isExe) {
            return &asset;
        }
        if (isZip && !zip) {
            zip = &asset;
        }
        if (isExe && !exe) {
            exe = &asset;
        }
    }
    return preferExe ? exe : (zip ? zip : exe);
}

bool HasUpdate(const InstalledApp& installed, const AppInfo& app) {
    if (!app.hasRelease) {
        return false;
    }
    return util::CompareVersions(app.release.tag, installed.version) > 0;
}
