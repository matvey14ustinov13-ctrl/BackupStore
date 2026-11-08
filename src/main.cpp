#include "app.h"

#include "catalog.h"
#include "install.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <memory>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")

namespace {

constexpr wchar_t kWindowClass[] = L"BackupStoreMainWindow";
constexpr UINT_PTR kCaretTimer = 1;
constexpr UINT_PTR kBootstrapTimer = 2;

AppState g_state;

void Redraw(HWND hwnd) {
    InvalidateRect(hwnd, nullptr, FALSE);
}

void PostStatus(HWND hwnd, const std::wstring& text, bool error = false) {
    PostMessageW(hwnd, WM_STORE_STATUS, 0, reinterpret_cast<LPARAM>(new StatusPayload{ text, error }));
}

void SetStatus(AppState& state, const std::wstring& text, bool error = false) {
    state.status = text;
    state.statusError = error;
    Redraw(state.hwnd);
}

void OpenUrl(const std::wstring& url) {
    if (!url.empty()) {
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void ShowError(HWND hwnd, const std::wstring& text, const wchar_t* title) {
    MessageBoxW(hwnd, text.c_str(), title, MB_OK | MB_ICONERROR);
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
    COLORREF caption = RGB(14, 17, 23);
    COLORREF text = RGB(232, 234, 237);
    COLORREF border = RGB(32, 38, 50);
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
    DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
}

void StartCatalogFetch(HWND hwnd, bool preferApi) {
    std::thread([hwnd, preferApi]() {
        auto status = [hwnd](const std::wstring& text) {
            PostStatus(hwnd, text);
        };

        std::wstring error;
        std::optional<Catalog> catalog;
        bool fromApi = false;

        if (!preferApi) {
            catalog = FetchCatalogFromRaw(status, error);
        }
        if (!catalog) {
            std::wstring apiError;
            catalog = FetchCatalogFromApi(status, apiError);
            fromApi = true;
            if (!catalog && error.empty()) {
                error = apiError;
            }
        }

        auto* payload = new CatalogPayload();
        payload->fromApi = fromApi;
        if (catalog) {
            payload->ok = true;
            payload->catalog = std::move(*catalog);
        } else {
            payload->ok = false;
            payload->error = error.empty() ? L"Не удалось загрузить каталог" : error;
        }
        PostMessageW(hwnd, WM_STORE_CATALOG, 0, reinterpret_cast<LPARAM>(payload));
    }).detach();
}

void StartStoreCheck(HWND hwnd, bool manual) {
    std::thread([hwnd, manual]() {
        std::wstring error;
        auto release = FetchLatestRelease(kOwner, kStoreRepo, error);

        auto* payload = new StoreUpdatePayload();
        payload->manual = manual;
        if (release) {
            payload->ok = true;
            payload->release = std::move(*release);
        } else {
            payload->ok = false;
            payload->error = error;
        }
        PostMessageW(hwnd, WM_STORE_STOREUPDATE, 0, reinterpret_cast<LPARAM>(payload));
    }).detach();
}

void StartInstall(HWND hwnd, const AppInfo& app) {
    if (g_state.installing || g_state.selfUpdating) {
        SetStatus(g_state, L"Дождитесь завершения текущей операции", true);
        return;
    }

    g_state.installing = true;
    g_state.installingRepo = app.repo;
    g_state.installPercent = 0;
    g_state.installText = L"Скачивание 0%";
    SetStatus(g_state, L"Установка " + app.repo + L"…");
    Redraw(hwnd);

    std::thread([hwnd, app]() {
        std::wstring repo = app.repo;
        auto progress = [hwnd, repo, last = -1](long long downloaded, long long total) mutable {
            int percent = total > 0 ? static_cast<int>(downloaded * 100 / total) : 0;
            if (percent == last) {
                return;
            }
            last = percent;
            PostMessageW(hwnd, WM_STORE_PROGRESS, 0,
                         reinterpret_cast<LPARAM>(new ProgressPayload{
                             repo, percent, L"Скачивание " + std::to_wstring(percent) + L"%"
                         }));
        };

        InstallResult result = InstallApp(app, progress);

        auto* payload = new OpDonePayload();
        payload->repo = repo;
        payload->ok = result.ok;
        payload->error = result.error;
        payload->version = result.version;
        payload->exe = result.exe;
        payload->selfUpdate = false;
        PostMessageW(hwnd, WM_STORE_OPDONE, 0, reinterpret_cast<LPARAM>(payload));
    }).detach();
}

void StartSelfUpdate(HWND hwnd) {
    if (g_state.installing || g_state.selfUpdating) {
        SetStatus(g_state, L"Дождитесь завершения текущей операции", true);
        return;
    }
    if (!g_state.storeRelease) {
        SetStatus(g_state, L"Нет данных об обновлении BackupStore", true);
        return;
    }

    ReleaseInfo release = *g_state.storeRelease;
    g_state.selfUpdating = true;
    g_state.installPercent = 0;
    g_state.installText = L"Скачивание 0%";
    SetStatus(g_state, L"Загрузка обновления BackupStore…");
    Redraw(hwnd);

    std::thread([hwnd, release]() {
        auto progress = [hwnd, last = -1](long long downloaded, long long total) mutable {
            int percent = total > 0 ? static_cast<int>(downloaded * 100 / total) : 0;
            if (percent == last) {
                return;
            }
            last = percent;
            PostMessageW(hwnd, WM_STORE_PROGRESS, 0,
                         reinterpret_cast<LPARAM>(new ProgressPayload{
                             kStoreRepo, percent, L"Скачивание " + std::to_wstring(percent) + L"%"
                         }));
        };

        std::wstring error;
        bool ok = PrepareSelfUpdate(release, progress, error);

        auto* payload = new OpDonePayload();
        payload->repo = kStoreRepo;
        payload->ok = ok;
        payload->error = error;
        payload->version = release.tag;
        payload->selfUpdate = true;
        PostMessageW(hwnd, WM_STORE_OPDONE, 0, reinterpret_cast<LPARAM>(payload));
    }).detach();
}

void UpdateStoreRelease(StoreUpdatePayload& payload) {
    g_state.checkingStoreUpdate = false;
    if (!payload.ok) {
        if (payload.manual) {
            SetStatus(g_state, L"Не удалось проверить обновления: " + payload.error, true);
        }
        Redraw(g_state.hwnd);
        return;
    }

    bool newer = util::CompareVersions(payload.release.tag, kAppVersionTag) > 0;
    g_state.storeRelease = payload.release;
    g_state.storeUpdateAvailable = newer;
    if (newer) {
        SetStatus(g_state, L"Доступно обновление BackupStore " + payload.release.tag);
    } else if (payload.manual) {
        SetStatus(g_state, L"Установлена последняя версия BackupStore");
    }
    Redraw(g_state.hwnd);
}

void HandleOpDone(OpDonePayload& payload) {
    if (payload.selfUpdate) {
        g_state.selfUpdating = false;
        g_state.installPercent = -1;
        g_state.installText.clear();
        if (payload.ok) {
            g_state.status = L"BackupStore обновляется и будет перезапущен…";
            g_state.statusError = false;
            Redraw(g_state.hwnd);
            PostQuitMessage(0);
        } else {
            SetStatus(g_state, L"Не удалось обновить BackupStore: " + payload.error, true);
            ShowError(g_state.hwnd, payload.error, L"Обновление BackupStore");
        }
        return;
    }

    g_state.installing = false;
    g_state.installingRepo.clear();
    g_state.installPercent = -1;
    g_state.installText.clear();
    g_state.installed = LoadInstalled();

    if (payload.ok) {
        SetStatus(g_state, L"«" + payload.repo + L"» установлен (" + payload.version + L")");
    } else {
        SetStatus(g_state, L"Ошибка установки «" + payload.repo + L"»: " + payload.error, true);
        ShowError(g_state.hwnd, payload.error, L"Не удалось установить приложение");
    }
    Redraw(g_state.hwnd);
}

void OpenRelease(const std::wstring& repo) {
    if (repo.empty() && g_state.storeRelease) {
        OpenUrl(g_state.storeRelease->htmlUrl);
        return;
    }
    const AppInfo* app = FindStoreApp(g_state, repo);
    if (!app) {
        return;
    }
    OpenUrl(app->release.htmlUrl.empty() ? app->htmlUrl : app->release.htmlUrl);
}

void HandleAction(const HitTarget& target) {
    switch (target.action) {
        case ACT_NAV:
            g_state.view = static_cast<View>(target.param);
            g_state.scroll = 0;
            g_state.searchFocused = false;
            KillTimer(g_state.hwnd, kCaretTimer);
            Redraw(g_state.hwnd);
            break;

        case ACT_REFRESH:
            if (!g_state.loadingCatalog) {
                g_state.loadingCatalog = true;
                SetStatus(g_state, L"Обновление каталога…");
                StartCatalogFetch(g_state.hwnd, true);
            }
            break;

        case ACT_SEARCH:
            g_state.searchFocused = true;
            g_state.caretVisible = true;
            SetTimer(g_state.hwnd, kCaretTimer, 530, nullptr);
            Redraw(g_state.hwnd);
            break;

        case ACT_INSTALL: {
            const AppInfo* app = FindStoreApp(g_state, target.arg);
            if (app) {
                StartInstall(g_state.hwnd, *app);
            } else {
                SetStatus(g_state, L"Приложение не найдено в каталоге", true);
            }
            break;
        }
        case ACT_LAUNCH: {
            auto it = g_state.installed.find(target.arg);
            if (it == g_state.installed.end()) {
                SetStatus(g_state, L"Приложение не установлено", true);
                break;
            }
            std::wstring error;
            if (!LaunchInstalled(it->second, error)) {
                SetStatus(g_state, error, true);
                ShowError(g_state.hwnd, error, L"Запуск приложения");
            }
            break;
        }

        case ACT_FOLDER: {
            auto it = g_state.installed.find(target.arg);
            if (it != g_state.installed.end()) {
                if (!OpenFolder(it->second.path)) {
                    SetStatus(g_state, L"Не удалось открыть папку", true);
                }
            } else if (!OpenFolder(util::AppsDir())) {
                SetStatus(g_state, L"Не удалось открыть папку", true);
            }
            break;
        }

        case ACT_UNINSTALL: {
            auto it = g_state.installed.find(target.arg);
            if (it == g_state.installed.end()) {
                break;
            }
            std::wstring question = L"Удалить «" + (it->second.title.empty() ? target.arg : it->second.title) +
                                    L"» и все его файлы?";
            if (MessageBoxW(g_state.hwnd, question.c_str(), L"Удаление приложения",
                            MB_YESNO | MB_ICONQUESTION) != IDYES) {
                break;
            }
            std::wstring error;
            if (UninstallApp(g_state.installed, target.arg, error)) {
                SetStatus(g_state, L"«" + target.arg + L"» удалён");
            } else {
                SetStatus(g_state, error, true);
                ShowError(g_state.hwnd, error, L"Удаление приложения");
            }
            Redraw(g_state.hwnd);
            break;
        }

        case ACT_RELEASE:
            OpenRelease(target.arg);
            break;

        case ACT_SELF_UPDATE:
            StartSelfUpdate(g_state.hwnd);
            break;

        case ACT_CHECK_STORE_UPDATE:
            if (!g_state.checkingStoreUpdate) {
                g_state.checkingStoreUpdate = true;
                SetStatus(g_state, L"Проверка обновлений…");
                StartStoreCheck(g_state.hwnd, true);
                Redraw(g_state.hwnd);
            }
            break;

        case ACT_OPEN_APPS:
            if (!OpenFolder(util::AppsDir())) {
                SetStatus(g_state, L"Не удалось открыть папку установки", true);
            }
            break;

        case ACT_OPEN_DOWNLOADS:
            if (!OpenFolder(util::DownloadsDir())) {
                SetStatus(g_state, L"Не удалось открыть папку загрузок", true);
            }
            break;

        case ACT_GITHUB:
            OpenUrl(L"https://github.com/matvey14ustinov13-ctrl");
            break;

        case ACT_LICENSE:
            OpenUrl(L"https://www.gnu.org/licenses/gpl-3.0.html");
            break;

        default:
            break;
    }
}

void HandleCatalog(CatalogPayload& payload) {
    g_state.loadingCatalog = false;
    if (!payload.ok) {
        if (g_state.apps.empty()) {
            SetStatus(g_state, L"Не удалось загрузить каталог: " + payload.error, true);
        } else {
            SetStatus(g_state, L"Не удалось обновить каталог, показаны сохранённые данные", true);
        }
        Redraw(g_state.hwnd);
        return;
    }

    g_state.catalog = std::move(payload.catalog);
    g_state.apps = g_state.catalog.apps;
    SetStatus(g_state, payload.fromApi ? L"Каталог обновлён через GitHub API" : L"Каталог обновлён");
    Redraw(g_state.hwnd);
}

void ClampScroll(AppState& state) {
    int viewport = state.contentRect.bottom - state.contentRect.top;
    int maxScroll = (std::max)(0, state.contentHeight - viewport);
    if (state.scroll < 0) {
        state.scroll = 0;
    }
    if (state.scroll > maxScroll) {
        state.scroll = maxScroll;
    }
}

void ScrollBy(AppState& state, int delta) {
    state.scroll += delta;
    ClampScroll(state);
    Redraw(state.hwnd);
}

void EndSearchFocus(AppState& state) {
    if (state.searchFocused) {
        state.searchFocused = false;
        KillTimer(state.hwnd, kCaretTimer);
        state.caretVisible = true;
        Redraw(state.hwnd);
    }
}

void OnMouseMove(AppState& state, POINT point) {
    state.mouse = point;
    state.mouseInside = true;

    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = state.hwnd;
    TrackMouseEvent(&track);

    HitTarget target;
    std::wstring key;
    if (HitTest(state, point, target)) {
        key = TargetKey(target);
    }
    if (key != state.hoverKey) {
        state.hoverKey = key;
        Redraw(state.hwnd);
    }

    if (!state.hoverKey.empty()) {
        if (target.action == ACT_SEARCH) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        }
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

void OnClick(AppState& state, POINT point) {
    SetFocus(state.hwnd);
    HitTarget target;
    if (!HitTest(state, point, target)) {
        EndSearchFocus(state);
        return;
    }

    if (target.action != ACT_SEARCH) {
        EndSearchFocus(state);
    }
    HandleAction(target);
}

void OnChar(AppState& state, wchar_t ch) {
    if (!state.searchFocused) {
        return;
    }
    if (ch == L'\b') {
        if (!state.search.empty()) {
            state.search.pop_back();
            state.scroll = 0;
            Redraw(state.hwnd);
        }
        return;
    }
    if (ch == L'\r' || ch == L'\n') {
        EndSearchFocus(state);
        return;
    }
    if (ch == 27) {
        state.search.clear();
        state.scroll = 0;
        EndSearchFocus(state);
        return;
    }
    if (ch >= 32 && ch != 127) {
        state.search.push_back(ch);
        state.scroll = 0;
        Redraw(state.hwnd);
    }
}

void OnKeyDown(AppState& state, WPARAM key) {
    if (key == VK_F5 && !state.loadingCatalog) {
        state.loadingCatalog = true;
        SetStatus(state, L"Обновление каталога…");
        StartCatalogFetch(state.hwnd, true);
        return;
    }

    if (state.searchFocused && key == VK_ESCAPE) {
        state.search.clear();
        state.scroll = 0;
        EndSearchFocus(state);
        return;
    }

    if (key == VK_PRIOR) {
        ScrollBy(state, -(state.contentRect.bottom - state.contentRect.top) * 9 / 10);
    } else if (key == VK_NEXT) {
        ScrollBy(state, (state.contentRect.bottom - state.contentRect.top) * 9 / 10);
    } else if (key == VK_HOME) {
        state.scroll = 0;
        Redraw(state.hwnd);
    } else if (key == VK_END) {
        state.scroll = state.contentHeight;
        ClampScroll(state);
        Redraw(state.hwnd);
    }
}

void InitializeState(HWND hwnd) {
    g_state = AppState();
    g_state.hwnd = hwnd;
    g_state.dpi = 96;

    typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto getDpi = reinterpret_cast<GetDpiForWindowFn>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForWindow")));
        if (getDpi) {
            UINT dpi = getDpi(hwnd);
            if (dpi >= 72 && dpi <= 480) {
                g_state.dpi = dpi;
            }
        }
    }

    CreateUiFonts(g_state);
    g_state.installed = LoadInstalled();
    g_state.installPercent = -1;

    std::wstring error;
    auto cached = LoadCachedCatalog(error);
    if (cached) {
        g_state.catalog = *cached;
        g_state.apps = cached->apps;
        g_state.status = L"Каталог загружен из кэша";
    } else {
        g_state.status = L"Загрузка каталога…";
    }

    g_state.loadingCatalog = true;
    SetTimer(hwnd, kBootstrapTimer, 40, nullptr);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&g_state));
            InitializeState(hwnd);
            ApplyDarkTitleBar(hwnd);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == kBootstrapTimer) {
                KillTimer(hwnd, kBootstrapTimer);
                StartCatalogFetch(hwnd, false);
                g_state.checkingStoreUpdate = true;
                StartStoreCheck(hwnd, false);
                return 0;
            }
            if (wParam == kCaretTimer) {
                g_state.caretVisible = !g_state.caretVisible;
                InvalidateRect(hwnd, &g_state.searchRect, FALSE);
                return 0;
            }
            break;
        }

        case WM_SIZE: {
            ClampScroll(g_state);
            Redraw(hwnd);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            int dpi = g_state.dpi ? static_cast<int>(g_state.dpi) : 96;
            info->ptMinTrackSize.x = MulDiv(980, dpi, 96);
            info->ptMinTrackSize.y = MulDiv(640, dpi, 96);
            return 0;
        }

        case WM_DPICHANGED: {
            g_state.dpi = HIWORD(wParam);
            CreateUiFonts(g_state);
            RECT* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            Redraw(hwnd);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);
            PaintUi(g_state, dc, client);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            OnMouseMove(g_state, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
            return 0;

        case WM_MOUSELEAVE:
            g_state.mouseInside = false;
            g_state.mouse = POINT{ -1, -1 };
            if (!g_state.hoverKey.empty()) {
                g_state.hoverKey.clear();
                Redraw(hwnd);
            }
            return 0;

        case WM_LBUTTONDOWN:
            OnClick(g_state, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
            return 0;

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            ScrollBy(g_state, -delta * g_state.D(80) / 120);
            return 0;
        }

        case WM_CHAR:
            OnChar(g_state, static_cast<wchar_t>(wParam));
            return 0;

        case WM_KEYDOWN:
            OnKeyDown(g_state, wParam);
            return 0;

        case WM_KILLFOCUS:
            EndSearchFocus(g_state);
            return 0;

        case WM_ACTIVATEAPP:
            if (wParam == TRUE && !g_state.installing && !g_state.selfUpdating) {
                g_state.installed = LoadInstalled();
                Redraw(hwnd);
            }
            return 0;

        case WM_STORE_STATUS: {
            std::unique_ptr<StatusPayload> payload(reinterpret_cast<StatusPayload*>(lParam));
            if (payload) {
                g_state.status = payload->text;
                g_state.statusError = payload->error;
                Redraw(hwnd);
            }
            return 0;
        }

        case WM_STORE_CATALOG: {
            std::unique_ptr<CatalogPayload> payload(reinterpret_cast<CatalogPayload*>(lParam));
            if (payload) {
                HandleCatalog(*payload);
            }
            return 0;
        }

        case WM_STORE_PROGRESS: {
            std::unique_ptr<ProgressPayload> payload(reinterpret_cast<ProgressPayload*>(lParam));
            if (payload) {
                g_state.installingRepo = payload->repo;
                g_state.installPercent = payload->percent;
                g_state.installText = payload->text;
                Redraw(hwnd);
            }
            return 0;
        }

        case WM_STORE_OPDONE: {
            std::unique_ptr<OpDonePayload> payload(reinterpret_cast<OpDonePayload*>(lParam));
            if (payload) {
                HandleOpDone(*payload);
            }
            return 0;
        }

        case WM_STORE_STOREUPDATE: {
            std::unique_ptr<StoreUpdatePayload> payload(reinterpret_cast<StoreUpdatePayload*>(lParam));
            if (payload) {
                UpdateStoreRelease(*payload);
            }
            return 0;
        }

        case WM_CLOSE:
            if (g_state.installing) {
                if (MessageBoxW(hwnd, L"Установка ещё не завершена. Закрыть BackupStore?",
                                L"BackupStore", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                    return 0;
                }
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kCaretTimer);
            KillTimer(hwnd, kBootstrapTimer);
            DestroyUiFonts(g_state);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    CleanupOldVersion();

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&windowClass)) {
        return 1;
    }

    int width = 1180;
    int height = 780;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;

    HWND hwnd = CreateWindowExW(0, kWindowClass, L"BackupStore — магазин приложений",
                                WS_OVERLAPPEDWINDOW, x, y, width, height,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, showCommand == SW_SHOWDEFAULT ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();
    return 0;
}
