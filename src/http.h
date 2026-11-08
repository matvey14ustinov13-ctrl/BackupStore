#pragma once

#include "common.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string body;
    std::wstring error;
    long long contentLength = 0;
};

using HttpProgress = std::function<void(long long downloaded, long long total)>;

HttpResult HttpGet(const std::wstring& url,
                   const std::vector<std::wstring>& headers = std::vector<std::wstring>());

HttpResult HttpDownload(const std::wstring& url,
                        const std::wstring& destination,
                        const HttpProgress& progress = HttpProgress(),
                        const std::atomic<bool>* cancel = nullptr);
