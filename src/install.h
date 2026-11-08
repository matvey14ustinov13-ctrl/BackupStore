#pragma once

#include "catalog.h"

#include <functional>
#include <string>

struct InstallResult {
    bool ok = false;
    std::wstring error;
    std::wstring path;
    std::wstring exe;
    std::wstring version;
};

using InstallProgress = std::function<void(long long downloaded, long long total)>;

InstallResult InstallApp(const AppInfo& app, const InstallProgress& progress);

bool ExtractZip(const std::wstring& archivePath,
                const std::wstring& destinationDir,
                std::wstring& error);

bool LaunchInstalled(const InstalledApp& app, std::wstring& error);
bool OpenFolder(const std::wstring& path);
bool UninstallApp(InstalledMap& installed, const std::wstring& repo, std::wstring& error);

bool PrepareSelfUpdate(const ReleaseInfo& release,
                       const InstallProgress& progress,
                       std::wstring& error);
void CleanupOldVersion();
