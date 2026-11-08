#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace util {

std::wstring Utf8ToWide(const std::string& value);
std::string WideToUtf8(const std::wstring& value);

std::wstring ToLower(std::wstring value);
bool StartsWithI(const std::wstring& value, const std::wstring& prefix);
bool ContainsI(const std::wstring& value, const std::wstring& needle);
std::wstring Trim(const std::wstring& value);

std::wstring JoinPath(const std::wstring& base, const std::wstring& child);
std::wstring LocalAppDataDir();
std::wstring DataDir();
std::wstring CacheDir();
std::wstring DownloadsDir();
std::wstring AppsDir();
std::wstring UpdateDir();
std::wstring InstalledFilePath();
std::wstring CatalogCachePath();
bool EnsureDir(const std::wstring& path);
bool FileExists(const std::wstring& path);
bool DirExists(const std::wstring& path);
bool RemoveTree(const std::wstring& path);
long long FileSize(const std::wstring& path);

std::wstring ExePath();
std::wstring ExeDir();
std::wstring ProcessName();
std::wstring NowIso8601();
std::wstring FormatDate(const std::wstring& iso);
std::wstring FormatSize(long long bytes);
std::wstring SanitizeName(const std::wstring& value);

int CompareVersions(const std::wstring& left, const std::wstring& right);
std::wstring NormalizeVersion(const std::wstring& tag);

} // namespace util
