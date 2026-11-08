#include "http.h"

#include <winhttp.h>

#include <cstdio>
#include <memory>

namespace {

struct InternetHandle {
    HINTERNET handle = nullptr;
    ~InternetHandle() {
        if (handle) {
            WinHttpCloseHandle(handle);
        }
    }
};

std::wstring WinHttpError(DWORD code) {
    wchar_t* buffer = nullptr;
    HMODULE module = GetModuleHandleW(L"winhttp.dll");
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    if (module) {
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
    }
    DWORD size = FormatMessageW(flags, module, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message;
    if (size && buffer) {
        message.assign(buffer, size);
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
            message.pop_back();
        }
    }
    if (buffer) {
        LocalFree(buffer);
    }
    if (message.empty()) {
        message = L"Ошибка WinHTTP " + std::to_wstring(code);
    }
    return message;
}

struct ParsedUrl {
    bool secure = false;
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
};

bool ParseUrl(const std::wstring& url, ParsedUrl& out, std::wstring& error) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
        error = L"Некорректный адрес: " + url;
        return false;
    }

    out.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    out.port = parts.nPort;
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength > 0 && parts.lpszExtraInfo) {
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (path.empty()) {
        path = L"/";
    }
    out.path = path;
    return true;
}

struct Session {
    InternetHandle session;
    InternetHandle connection;
    InternetHandle request;
    ParsedUrl url;
};

bool OpenRequest(const std::wstring& address,
                 const std::vector<std::wstring>& headers,
                 Session& session,
                 std::wstring& error) {
    if (!ParseUrl(address, session.url, error)) {
        return false;
    }

    session.session.handle = WinHttpOpen(L"BackupStore/1.0 (Windows)",
                                        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
    if (!session.session.handle) {
        session.session.handle = WinHttpOpen(L"BackupStore/1.0 (Windows)",
                                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                             WINHTTP_NO_PROXY_NAME,
                                             WINHTTP_NO_PROXY_BYPASS,
                                             0);
    }
    if (!session.session.handle) {
        error = WinHttpError(GetLastError());
        return false;
    }

    WinHttpSetTimeouts(session.session.handle, 20000, 20000, 30000, 60000);

    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session.session.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

    session.connection.handle = WinHttpConnect(session.session.handle,
                                               session.url.host.c_str(),
                                               session.url.port,
                                               0);
    if (!session.connection.handle) {
        error = WinHttpError(GetLastError());
        return false;
    }

    DWORD flags = session.url.secure ? WINHTTP_FLAG_SECURE : 0;
    session.request.handle = WinHttpOpenRequest(session.connection.handle,
                                                L"GET",
                                                session.url.path.c_str(),
                                                nullptr,
                                                WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                flags);
    if (!session.request.handle) {
        error = WinHttpError(GetLastError());
        return false;
    }

    for (const std::wstring& header : headers) {
        if (!header.empty()) {
            WinHttpAddRequestHeaders(session.request.handle,
                                     header.c_str(),
                                     static_cast<DWORD>(header.size()),
                                     WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
    }

    if (!WinHttpSendRequest(session.request.handle,
                            WINHTTP_NO_ADDITIONAL_HEADERS,
                            0,
                            WINHTTP_NO_REQUEST_DATA,
                            0,
                            0,
                            0)) {
        error = WinHttpError(GetLastError());
        return false;
    }

    if (!WinHttpReceiveResponse(session.request.handle, nullptr)) {
        error = WinHttpError(GetLastError());
        return false;
    }

    return true;
}

int ReadStatus(HINTERNET request) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status,
                            &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        return static_cast<int>(status);
    }
    return 0;
}

long long ReadContentLength(HINTERNET request) {
    wchar_t buffer[64]{};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            buffer,
                            &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        return _wtoi64(buffer);
    }
    return -1;
}

} // namespace

HttpResult HttpGet(const std::wstring& url, const std::vector<std::wstring>& headers) {
    HttpResult result;
    Session session;
    std::wstring error;
    if (!OpenRequest(url, headers, session, error)) {
        result.error = error;
        return result;
    }

    result.status = ReadStatus(session.request.handle);
    if (result.status < 200 || result.status >= 300) {
        result.error = L"HTTP " + std::to_wstring(result.status) + L" при запросе " + url;
        return result;
    }

    result.contentLength = ReadContentLength(session.request.handle);
    char buffer[16384];
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(session.request.handle, &available)) {
            result.error = WinHttpError(GetLastError());
            return result;
        }
        if (available == 0) {
            break;
        }
        DWORD toRead = available;
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }
        DWORD read = 0;
        if (!WinHttpReadData(session.request.handle, buffer, toRead, &read)) {
            result.error = WinHttpError(GetLastError());
            return result;
        }
        if (read == 0) {
            break;
        }
        result.body.append(buffer, read);
    }

    result.ok = true;
    return result;
}

HttpResult HttpDownload(const std::wstring& url,
                        const std::wstring& destination,
                        const HttpProgress& progress,
                        const std::atomic<bool>* cancel) {
    HttpResult result;
    Session session;
    std::wstring error;
    if (!OpenRequest(url, std::vector<std::wstring>{ L"Accept: application/octet-stream" }, session, error)) {
        result.error = error;
        return result;
    }

    result.status = ReadStatus(session.request.handle);
    if (result.status < 200 || result.status >= 300) {
        result.error = L"HTTP " + std::to_wstring(result.status) + L" при скачивании " + url;
        return result;
    }

    result.contentLength = ReadContentLength(session.request.handle);

    FILE* file = nullptr;
    errno_t openResult = _wfopen_s(&file, destination.c_str(), L"wb");
    if (openResult != 0 || !file) {
        result.error = L"Не удалось создать файл " + destination;
        return result;
    }

    long long downloaded = 0;
    char buffer[65536];
    bool failed = false;

    for (;;) {
        if (cancel && cancel->load()) {
            result.error = L"Загрузка отменена";
            failed = true;
            break;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(session.request.handle, &available)) {
            result.error = WinHttpError(GetLastError());
            failed = true;
            break;
        }
        if (available == 0) {
            break;
        }
        DWORD toRead = available;
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }
        DWORD read = 0;
        if (!WinHttpReadData(session.request.handle, buffer, toRead, &read)) {
            result.error = WinHttpError(GetLastError());
            failed = true;
            break;
        }
        if (read == 0) {
            break;
        }
        if (fwrite(buffer, 1, read, file) != read) {
            result.error = L"Ошибка записи файла " + destination;
            failed = true;
            break;
        }
        downloaded += read;
        if (progress) {
            progress(downloaded, result.contentLength);
        }
    }

    fclose(file);

    if (failed) {
        DeleteFileW(destination.c_str());
        return result;
    }

    result.ok = true;
    return result;
}
