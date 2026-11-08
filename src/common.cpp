#include "common.h"

#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <sstream>

namespace util {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::string();
    }
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool StartsWithI(const std::wstring& value, const std::wstring& prefix) {
    if (prefix.size() > value.size()) {
        return false;
    }
    return _wcsnicmp(value.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool ContainsI(const std::wstring& value, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    return ToLower(value).find(ToLower(needle)) != std::wstring::npos;
}

std::wstring Trim(const std::wstring& value) {
    size_t first = 0;
    while (first < value.size() && std::iswspace(value[first])) {
        ++first;
    }
    size_t last = value.size();
    while (last > first && std::iswspace(value[last - 1])) {
        --last;
    }
    return value.substr(first, last - first);
}

std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
    if (base.empty()) {
        return child;
    }
    if (child.empty()) {
        return base;
    }
    wchar_t last = base.back();
    if (last == L'\\' || last == L'/') {
        return base + child;
    }
    return base + L"\\" + child;
}

std::wstring LocalAppDataDir() {
    PWSTR raw = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        result = raw;
    }
    if (raw) {
        CoTaskMemFree(raw);
    }
    if (result.empty()) {
        wchar_t buffer[MAX_PATH]{};
        DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
        if (size > 0 && size < MAX_PATH) {
            result = buffer;
        }
    }
    return result;
}

std::wstring DataDir() {
    return JoinPath(LocalAppDataDir(), L"BackupStore");
}

std::wstring CacheDir() {
    return JoinPath(DataDir(), L"cache");
}

std::wstring DownloadsDir() {
    return JoinPath(DataDir(), L"downloads");
}

std::wstring AppsDir() {
    return JoinPath(DataDir(), L"apps");
}

std::wstring UpdateDir() {
    return JoinPath(DataDir(), L"update");
}

std::wstring InstalledFilePath() {
    return JoinPath(DataDir(), L"installed.json");
}

std::wstring CatalogCachePath() {
    return JoinPath(CacheDir(), L"catalog.json");
}

bool EnsureDir(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return std::filesystem::is_directory(path, ec);
    }
    return std::filesystem::create_directories(path, ec);
}

bool FileExists(const std::wstring& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

bool DirExists(const std::wstring& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_directory(path, ec);
}

bool RemoveTree(const std::wstring& path) {
    if (path.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    return !ec;
}

long long FileSize(const std::wstring& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return -1;
    }
    return static_cast<long long>(size);
}

std::wstring ExePath() {
    wchar_t buffer[MAX_PATH * 4]{};
    DWORD size = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    return std::wstring(buffer, size);
}

std::wstring ExeDir() {
    std::wstring path = ExePath();
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring ProcessName() {
    std::wstring path = ExePath();
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring NowIso8601() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%04u-%02u-%02uT%02u:%02u:%02uZ", st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

std::wstring FormatDate(const std::wstring& iso) {
    if (iso.size() < 10) {
        return iso;
    }
    // YYYY-MM-DD -> DD.MM.YYYY
    if (iso[4] == L'-' && iso[7] == L'-') {
        return iso.substr(8, 2) + L"." + iso.substr(5, 2) + L"." + iso.substr(0, 4);
    }
    return iso.substr(0, 10);
}

std::wstring FormatSize(long long bytes) {
    if (bytes < 0) {
        return L"—";
    }
    const wchar_t* units[] = { L"Б", L"КБ", L"МБ", L"ГБ" };
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }
    wchar_t buffer[64]{};
    if (unit == 0) {
        swprintf_s(buffer, L"%lld %s", bytes, units[unit]);
    } else {
        swprintf_s(buffer, L"%.1f %s", value, units[unit]);
    }
    return buffer;
}

std::wstring SanitizeName(const std::wstring& value) {
    std::wstring result;
    for (wchar_t ch : value) {
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
            (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-' || ch == L'_' || ch == L' ') {
            result.push_back(ch);
        } else {
            result.push_back(L'_');
        }
    }
    while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) {
        result.pop_back();
    }
    if (result.empty()) {
        result = L"app";
    }
    return result;
}

std::wstring NormalizeVersion(const std::wstring& tag) {
    std::wstring value = Trim(tag);
    size_t start = 0;
    while (start < value.size() && !std::iswdigit(value[start])) {
        ++start;
    }
    return value.substr(start);
}

int CompareVersions(const std::wstring& left, const std::wstring& right) {
    std::wstring a = NormalizeVersion(left);
    std::wstring b = NormalizeVersion(right);
    size_t ai = 0;
    size_t bi = 0;
    while (ai < a.size() || bi < b.size()) {
        long long an = 0;
        long long bn = 0;
        while (ai < a.size() && std::iswdigit(a[ai])) {
            an = an * 10 + (a[ai] - L'0');
            ++ai;
        }
        while (bi < b.size() && std::iswdigit(b[bi])) {
            bn = bn * 10 + (b[bi] - L'0');
            ++bi;
        }
        if (an != bn) {
            return an < bn ? -1 : 1;
        }
        while (ai < a.size() && !std::iswdigit(a[ai])) {
            ++ai;
        }
        while (bi < b.size() && !std::iswdigit(b[bi])) {
            ++bi;
        }
    }
    return 0;
}

} // namespace util
