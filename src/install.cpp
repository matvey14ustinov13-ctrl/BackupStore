#include "install.h"

#include "http.h"

#include <shellapi.h>

#include <miniz.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::wstring TimestampSuffix() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

bool IsDirectoryEntry(mz_zip_archive& zip, mz_uint index) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&zip, index, &stat)) {
        return false;
    }
    return stat.m_is_directory != MZ_FALSE;
}

std::wstring ZipEntryName(mz_zip_archive& zip, mz_uint index) {
    mz_uint size = mz_zip_reader_get_filename(&zip, index, nullptr, 0);
    if (size == 0) {
        return std::wstring();
    }
    std::vector<char> buffer(size + 1, '\0');
    mz_zip_reader_get_filename(&zip, index, buffer.data(), size + 1);
    return util::Utf8ToWide(std::string(buffer.data()));
}

std::wstring SafeRelativePath(const std::wstring& entry) {
    std::wstring path = entry;
    std::replace(path.begin(), path.end(), L'\\', L'/');
    if (path.empty() || path[0] == L'/') {
        return std::wstring();
    }
    if (path.size() >= 2 && path[1] == L':') {
        return std::wstring();
    }

    std::wstring result;
    size_t start = 0;
    while (start <= path.size()) {
        size_t slash = path.find(L'/', start);
        if (slash == std::wstring::npos) {
            slash = path.size();
        }
        std::wstring part = path.substr(start, slash - start);
        if (!part.empty() && part != L".") {
            if (part == L"..") {
                return std::wstring();
            }
            if (!result.empty()) {
                result += L'\\';
            }
            result += part;
        }
        start = slash + 1;
    }
    return result;
}

std::wstring FindMainExecutable(const std::wstring& root, const std::wstring& repo) {
    std::wstring best;
    int bestScore = 0;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        std::wstring name = it->path().filename().wstring();
        std::wstring lower = util::ToLower(name);
        if (lower.size() < 5 || lower.substr(lower.size() - 4) != L".exe") {
            continue;
        }
        if (lower.rfind(L"unins", 0) == 0 || lower.rfind(L"setup", 0) == 0 ||
            lower.rfind(L"vc_redist", 0) == 0) {
            continue;
        }

        int score = 0;
        if (_wcsicmp(name.c_str(), (repo + L".exe").c_str()) == 0) {
            score = 3;
        } else if (lower.find(util::ToLower(repo)) != std::wstring::npos) {
            score = 2;
        } else if (it->path().parent_path() == root) {
            score = 1;
        }
        if (score > bestScore) {
            best = it->path().wstring();
            bestScore = score;
            if (score == 3) {
                break;
            }
        }
    }
    return best;
}

} // namespace

bool ExtractZip(const std::wstring& archivePath, const std::wstring& destinationDir, std::wstring& error) {
    mz_zip_archive zip{};
    std::string archiveUtf8 = util::WideToUtf8(archivePath);
    if (!mz_zip_reader_init_file(&zip, archiveUtf8.c_str(), 0)) {
        error = L"Не удалось открыть ZIP-архив";
        return false;
    }

    bool ok = true;
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (IsDirectoryEntry(zip, i)) {
            continue;
        }
        std::wstring relative = SafeRelativePath(ZipEntryName(zip, i));
        if (relative.empty()) {
            continue;
        }
        std::wstring destination = util::JoinPath(destinationDir, relative);
        std::wstring parent = destination;
        size_t slash = parent.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            parent = parent.substr(0, slash);
            if (!util::EnsureDir(parent)) {
                error = L"Не удалось создать папку " + parent;
                ok = false;
                break;
            }
        }
        std::string destinationUtf8 = util::WideToUtf8(destination);
        if (!mz_zip_reader_extract_to_file(&zip, i, destinationUtf8.c_str(), 0)) {
            error = L"Не удалось распаковать файл " + relative;
            ok = false;
            break;
        }
    }

    mz_zip_reader_end(&zip);
    return ok;
}

InstallResult InstallApp(const AppInfo& app, const InstallProgress& progress) {
    InstallResult result;
    result.version = app.release.tag;

    const AssetInfo* asset = ChooseAsset(app.release);
    if (!asset) {
        result.error = L"В последнем релизе нет подходящего файла (.zip или .exe)";
        return result;
    }

    if (!util::EnsureDir(util::DownloadsDir()) || !util::EnsureDir(util::AppsDir())) {
        result.error = L"Не удалось создать папки приложения";
        return result;
    }

    std::wstring lower = util::ToLower(asset->name);
    bool isZip = lower.size() > 4 && lower.substr(lower.size() - 4) == L".zip";
    bool isExe = lower.size() > 4 && lower.substr(lower.size() - 4) == L".exe";
    if (!isZip && !isExe) {
        result.error = L"Формат файла не поддерживается: " + asset->name;
        return result;
    }

    std::wstring safeRepo = util::SanitizeName(app.repo);
    std::wstring safeVersion = util::SanitizeName(app.release.tag);
    std::wstring downloadPath = util::JoinPath(
        util::DownloadsDir(),
        safeRepo + L"-" + safeVersion + (isZip ? L".zip" : L".exe"));

    DeleteFileW(downloadPath.c_str());

    HttpResult download = HttpDownload(asset->url, downloadPath, progress);
    if (!download.ok) {
        result.error = download.error;
        return result;
    }

    long long actualSize = util::FileSize(downloadPath);
    if (actualSize <= 0) {
        DeleteFileW(downloadPath.c_str());
        result.error = L"Скачанный файл пуст";
        return result;
    }

    fs::path appRoot = fs::path(util::AppsDir()) / safeRepo;
    fs::path target = appRoot / (safeVersion.empty() ? L"latest" : safeVersion);
    if (fs::exists(target)) {
        util::RemoveTree(target.wstring());
    }
    if (fs::exists(target)) {
        target = appRoot / ((safeVersion.empty() ? L"latest" : safeVersion) + L"-" + TimestampSuffix());
    }

    if (!util::EnsureDir(target.wstring())) {
        result.error = L"Не удалось создать папку установки";
        return result;
    }

    std::wstring exePath;
    if (isZip) {
        std::wstring extractError;
        if (!ExtractZip(downloadPath, target.wstring(), extractError)) {
            util::RemoveTree(target.wstring());
            result.error = extractError;
            return result;
        }
        exePath = FindMainExecutable(target.wstring(), app.repo);
    } else {
        std::wstring destination = util::JoinPath(target.wstring(), asset->name);
        if (!CopyFileW(downloadPath.c_str(), destination.c_str(), FALSE)) {
            util::RemoveTree(target.wstring());
            result.error = L"Не удалось скопировать файл программы";
            return result;
        }
        exePath = destination;
    }

    InstalledMap installed = LoadInstalled();
    InstalledApp record;
    record.repo = app.repo;
    record.version = app.release.tag;
    record.path = target.wstring();
    record.exe = exePath;
    record.installedAt = util::NowIso8601();
    record.title = app.repo;
    installed[app.repo] = record;
    SaveInstalled(installed);

    // Удаляем старые версии этого приложения, если они не заняты запущенным процессом.
    std::error_code ec;
    for (fs::directory_iterator it(appRoot, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_directory(ec)) {
            continue;
        }
        if (it->path() == target) {
            continue;
        }
        std::error_code removeError;
        fs::remove_all(it->path(), removeError);
    }

    result.ok = true;
    result.path = target.wstring();
    result.exe = exePath;
    return result;
}

bool LaunchInstalled(const InstalledApp& app, std::wstring& error) {
    if (!util::FileExists(app.exe)) {
        error = L"Исполняемый файл не найден. Возможно, приложение было удалено.";
        return false;
    }

    std::wstring directory = app.path;
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = app.exe.c_str();
    info.lpDirectory = directory.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        error = L"Не удалось запустить приложение (код " + std::to_wstring(GetLastError()) + L")";
        return false;
    }
    return true;
}

bool OpenFolder(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    util::EnsureDir(path);
    HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool UninstallApp(InstalledMap& installed, const std::wstring& repo, std::wstring& error) {
    auto it = installed.find(repo);
    if (it == installed.end()) {
        error = L"Приложение не установлено";
        return false;
    }

    if (!util::RemoveTree(it->second.path)) {
        error = L"Не удалось удалить файлы. Закройте приложение и попробуйте снова.";
        return false;
    }

    fs::path appRoot = fs::path(util::AppsDir()) / util::SanitizeName(repo);
    std::error_code ec;
    if (fs::exists(appRoot, ec) && fs::is_directory(appRoot, ec)) {
        bool empty = fs::is_empty(appRoot, ec);
        if (empty && !ec) {
            fs::remove(appRoot, ec);
        }
    }

    installed.erase(it);
    SaveInstalled(installed);
    return true;
}

bool PrepareSelfUpdate(const ReleaseInfo& release, const InstallProgress& progress, std::wstring& error) {
    const AssetInfo* asset = ChooseAsset(release, true);
    if (!asset) {
        error = L"В релизе BackupStore нет исполняемого файла";
        return false;
    }

    if (!util::EnsureDir(util::UpdateDir())) {
        error = L"Не удалось создать папку обновления";
        return false;
    }

    std::wstring newExe = util::JoinPath(util::UpdateDir(), L"BackupStore-new.exe");
    DeleteFileW(newExe.c_str());

    HttpResult download = HttpDownload(asset->url, newExe, progress);
    if (!download.ok) {
        error = download.error;
        return false;
    }

    long long size = util::FileSize(newExe);
    if (size <= 0) {
        DeleteFileW(newExe.c_str());
        error = L"Скачанный файл обновления пуст";
        return false;
    }
    if (asset->size > 0 && size != asset->size) {
        DeleteFileW(newExe.c_str());
        error = L"Размер обновления не совпадает с ожидаемым";
        return false;
    }

    std::wstring currentExe = util::ExePath();
    std::wstring script = util::JoinPath(util::UpdateDir(), L"apply-update.ps1");

    // The script itself stays ASCII-only; paths are passed as Unicode arguments.
    const wchar_t* scriptText =
        L"param(\r\n"
        L"    [Parameter(Mandatory=$true)][int]$TargetPid,\r\n"
        L"    [Parameter(Mandatory=$true)][string]$CurrentExe,\r\n"
        L"    [Parameter(Mandatory=$true)][string]$NewExe\r\n"
        L")\r\n"
        L"$ErrorActionPreference = 'Stop'\r\n"
        L"$deadline = (Get-Date).AddMinutes(5)\r\n"
        L"while ((Get-Date) -lt $deadline) {\r\n"
        L"    $process = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue\r\n"
        L"    if (-not $process) { break }\r\n"
        L"    Start-Sleep -Milliseconds 500\r\n"
        L"}\r\n"
        L"$old = $CurrentExe + '.old'\r\n"
        L"$done = $false\r\n"
        L"for ($attempt = 0; $attempt -lt 20 -and -not $done; $attempt++) {\r\n"
        L"    try {\r\n"
        L"        if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Force -ErrorAction SilentlyContinue }\r\n"
        L"        Move-Item -LiteralPath $CurrentExe -Destination $old -Force -ErrorAction Stop\r\n"
        L"        Copy-Item -LiteralPath $NewExe -Destination $CurrentExe -Force -ErrorAction Stop\r\n"
        L"        Start-Process -FilePath $CurrentExe\r\n"
        L"        $done = $true\r\n"
        L"    } catch {\r\n"
        L"        if (Test-Path -LiteralPath $old) {\r\n"
        L"            Move-Item -LiteralPath $old -Destination $CurrentExe -Force -ErrorAction SilentlyContinue\r\n"
        L"        }\r\n"
        L"        Start-Sleep -Milliseconds 750\r\n"
        L"    }\r\n"
        L"}\r\n"
        L"if ($done) {\r\n"
        L"    Start-Sleep -Milliseconds 500\r\n"
        L"    Remove-Item -LiteralPath $old -Force -ErrorAction SilentlyContinue\r\n"
        L"    Remove-Item -LiteralPath $NewExe -Force -ErrorAction SilentlyContinue\r\n"
        L"}\r\n"
        L"Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n";

    {
        std::ofstream file(script, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = L"Не удалось создать сценарий обновления";
            return false;
        }
        std::string utf8 = util::WideToUtf8(scriptText);
        file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    }

    std::wstring systemRoot;
    wchar_t systemRootBuffer[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"SystemRoot", systemRootBuffer, MAX_PATH) > 0) {
        systemRoot = systemRootBuffer;
    }
    std::wstring powershell = systemRoot.empty()
                                  ? L"powershell.exe"
                                  : systemRoot + L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (!util::FileExists(powershell)) {
        powershell = L"powershell.exe";
    }

    std::wstring command = L"\"" + powershell +
                           L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
                           script + L"\" -TargetPid " + std::to_wstring(GetCurrentProcessId()) +
                           L" -CurrentExe \"" + currentExe + L"\" -NewExe \"" + newExe + L"\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};

    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    if (!CreateProcessW(nullptr,
                        commandBuffer.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &startup,
                        &process)) {
        error = L"Не удалось запустить обновление (код " + std::to_wstring(GetLastError()) + L")";
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

void CleanupOldVersion() {
    std::wstring oldExe = util::ExePath() + L".old";
    for (int i = 0; i < 5; ++i) {
        if (!util::FileExists(oldExe)) {
            return;
        }
        if (DeleteFileW(oldExe.c_str())) {
            return;
        }
        Sleep(200);
    }
}
