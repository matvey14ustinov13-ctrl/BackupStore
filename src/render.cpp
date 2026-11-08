#include "app.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr COLORREF kBg = RGB(11, 13, 17);
constexpr COLORREF kHeader = RGB(14, 17, 23);
constexpr COLORREF kSidebar = RGB(14, 17, 23);
constexpr COLORREF kCard = RGB(20, 24, 33);
constexpr COLORREF kCardHover = RGB(25, 30, 42);
constexpr COLORREF kBorder = RGB(32, 38, 50);
constexpr COLORREF kBorderHover = RGB(54, 64, 82);
constexpr COLORREF kText = RGB(232, 234, 237);
constexpr COLORREF kMuted = RGB(139, 147, 167);
constexpr COLORREF kDim = RGB(95, 103, 122);
constexpr COLORREF kAccent = RGB(79, 140, 255);
constexpr COLORREF kAccentHover = RGB(107, 161, 255);
constexpr COLORREF kAccentSoft = RGB(30, 45, 76);
constexpr COLORREF kGreen = RGB(63, 185, 80);
constexpr COLORREF kGreenSoft = RGB(24, 47, 31);
constexpr COLORREF kOrange = RGB(210, 153, 34);
constexpr COLORREF kOrangeSoft = RGB(48, 38, 18);
constexpr COLORREF kDanger = RGB(248, 81, 73);
constexpr COLORREF kDangerSoft = RGB(44, 23, 26);
constexpr COLORREF kButton = RGB(27, 33, 46);
constexpr COLORREF kButtonHover = RGB(35, 42, 58);
constexpr COLORREF kWhite = RGB(255, 255, 255);

enum ButtonStyle {
    BUTTON_PRIMARY,
    BUTTON_SECONDARY,
    BUTTON_DANGER,
    BUTTON_GHOST
};

int Scale(const AppState& state, int value) {
    return state.D(value);
}

void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void FillRoundRectColor(HDC dc, const RECT& rect, int radius, COLORREF color) {
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }
    HRGN region = CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, radius * 2, radius * 2);
    HBRUSH brush = CreateSolidBrush(color);
    FillRgn(dc, region, brush);
    DeleteObject(brush);
    DeleteObject(region);
}

void FrameRoundRect(HDC dc, const RECT& rect, int radius, COLORREF color) {
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }
    HRGN region = CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, radius * 2, radius * 2);
    HBRUSH brush = CreateSolidBrush(color);
    FrameRgn(dc, region, brush, 1, 1);
    DeleteObject(brush);
    DeleteObject(region);
}

void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

int MeasureText(HDC dc, HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    return size.cx;
}

void DrawTextRect(HDC dc,
                  HFONT font,
                  const std::wstring& text,
                  RECT rect,
                  COLORREF color,
                  UINT flags) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, flags | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

std::wstring MakeKey(Action action, const std::wstring& arg, int param) {
    return std::to_wstring(static_cast<int>(action)) + L"|" + arg + L"|" + std::to_wstring(param);
}

bool IsHovered(const AppState& state, Action action, const std::wstring& arg, int param = 0) {
    return !state.hoverKey.empty() && state.hoverKey == MakeKey(action, arg, param);
}

void AddTarget(AppState& state, RECT rect, Action action, std::wstring arg, int param = 0) {
    HitTarget target;
    target.rect = rect;
    target.action = action;
    target.arg = std::move(arg);
    target.param = param;
    state.targets.push_back(std::move(target));
}

int ButtonWidth(const AppState& state, HDC dc, const std::wstring& label) {
    int width = MeasureText(dc, state.fontBodyBold, label) + Scale(state, 28);
    return (std::max)(Scale(state, 84), width);
}

void DrawButton(AppState& state,
                HDC dc,
                RECT rect,
                const std::wstring& label,
                ButtonStyle style,
                bool hovered,
                bool enabled,
                Action action = ACT_NONE,
                const std::wstring& arg = std::wstring(),
                int param = 0) {
    int radius = Scale(state, 6);
    if (enabled) {
        AddTarget(state, rect, action, arg, param);
    }

    COLORREF fill = kButton;
    COLORREF textColor = kText;
    COLORREF border = kBorder;
    bool drawBorder = true;

    switch (style) {
        case BUTTON_PRIMARY:
            fill = hovered && enabled ? kAccentHover : kAccent;
            textColor = kWhite;
            drawBorder = false;
            break;
        case BUTTON_SECONDARY:
            fill = hovered && enabled ? kButtonHover : kButton;
            textColor = enabled ? kText : kDim;
            break;
        case BUTTON_DANGER:
            fill = hovered && enabled ? kDangerSoft : kButton;
            textColor = enabled ? kDanger : kDim;
            border = hovered && enabled ? RGB(92, 38, 40) : kBorder;
            break;
        case BUTTON_GHOST:
            fill = hovered ? kCardHover : kBg;
            textColor = enabled ? kMuted : kDim;
            drawBorder = false;
            break;
    }

    if (!enabled) {
        if (style == BUTTON_PRIMARY) {
            fill = RGB(40, 52, 74);
        }
        textColor = kDim;
    }

    FillRoundRectColor(dc, rect, radius, fill);
    if (drawBorder) {
        FrameRoundRect(dc, rect, radius, border);
    }
    DrawTextRect(dc, state.fontBodyBold, label, rect, textColor,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawIconCatalog(HDC dc, const RECT& rect, COLORREF color) {
    int unit = (rect.right - rect.left) / 12;
    if (unit < 2) {
        unit = 2;
    }
    int x = rect.left;
    int y = rect.top;
    HBRUSH brush = CreateSolidBrush(color);
    RECT squares[4] = {
        { x, y, x + 4 * unit, y + 4 * unit },
        { x + 6 * unit, y, x + 10 * unit, y + 4 * unit },
        { x, y + 6 * unit, x + 4 * unit, y + 10 * unit },
        { x + 6 * unit, y + 6 * unit, x + 10 * unit, y + 10 * unit }
    };
    for (const RECT& square : squares) {
        FillRect(dc, &square, brush);
    }
    DeleteObject(brush);
}

void DrawIconCheck(HDC dc, const RECT& rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old = SelectObject(dc, pen);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    MoveToEx(dc, rect.left + w / 5, rect.top + h / 2, nullptr);
    LineTo(dc, rect.left + w * 2 / 5, rect.top + h * 3 / 4);
    LineTo(dc, rect.left + w * 4 / 5, rect.top + h / 4);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawIconArrowUp(HDC dc, const RECT& rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old = SelectObject(dc, pen);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int cx = rect.left + w / 2;
    MoveToEx(dc, cx, rect.top + h - 2, nullptr);
    LineTo(dc, cx, rect.top + 2);
    MoveToEx(dc, cx - w / 4, rect.top + h / 4, nullptr);
    LineTo(dc, cx, rect.top + 2);
    LineTo(dc, cx + w / 4, rect.top + h / 4);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawIconSettings(HDC dc, const RECT& rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old = SelectObject(dc, pen);
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    for (int row = 0; row < 3; ++row) {
        int y = rect.top + h / 6 + row * h / 3;
        MoveToEx(dc, rect.left, y, nullptr);
        LineTo(dc, rect.right, y);
        int knobX = rect.left + w * (row == 1 ? 2 : 3) / 5;
        MoveToEx(dc, knobX, y - h / 8, nullptr);
        LineTo(dc, knobX, y + h / 8);
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawIconSearch(HDC dc, const RECT& rect, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int size = (std::min)(rect.right - rect.left, rect.bottom - rect.top);
    Ellipse(dc, rect.left, rect.top, rect.left + size - 2, rect.top + size - 2);
    MoveToEx(dc, rect.left + size - 4, rect.top + size - 4, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    SelectObject(dc, oldBrush);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawIconDownload(HDC dc, const RECT& rect, COLORREF color) {
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    HBRUSH brush = CreateSolidBrush(color);
    RECT stem{ rect.left + w / 2 - (std::max)(2, w / 10), rect.top + 2,
               rect.left + w / 2 + (std::max)(2, w / 10), rect.top + h * 3 / 5 };
    FillRect(dc, &stem, brush);
    POINT points[3] = {
        { rect.left + w / 5, rect.top + h / 2 },
        { rect.right - w / 5, rect.top + h / 2 },
        { rect.left + w / 2, rect.top + h - 2 }
    };
    Polygon(dc, points, 3);
    RECT base{ rect.left + w / 6, rect.bottom - 2, rect.right - w / 6, rect.bottom };
    FillRect(dc, &base, brush);
    DeleteObject(brush);
}

void DrawBadge(AppState& state,
               HDC dc,
               int& x,
               int y,
               const std::wstring& text,
               COLORREF background,
               COLORREF foreground) {
    int height = Scale(state, 22);
    int width = MeasureText(dc, state.fontBadge, text) + Scale(state, 20);
    RECT rect{ x, y, x + width, y + height };
    FillRoundRectColor(dc, rect, Scale(state, 5), background);
    DrawTextRect(dc, state.fontBadge, text, rect, foreground,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += width + Scale(state, 8);
}

void DrawEmptyState(AppState& state, HDC dc, const RECT& area,
                    const std::wstring& title, const std::wstring& subtitle,
                    Action action = ACT_NONE, const std::wstring& actionLabel = std::wstring()) {
    int centerY = area.top + (area.bottom - area.top) / 2 - Scale(state, 50);
    int iconSize = Scale(state, 64);
    RECT icon{ (area.left + area.right) / 2 - iconSize / 2, centerY, 0, 0 };
    icon.right = icon.left + iconSize;
    icon.bottom = icon.top + iconSize;
    FillRoundRectColor(dc, icon, Scale(state, 16), kCard);
    FrameRoundRect(dc, icon, Scale(state, 16), kBorder);
    RECT glyph{ icon.left + iconSize / 4, icon.top + iconSize / 5,
                icon.right - iconSize / 4, icon.bottom - iconSize / 6 };
    DrawIconDownload(dc, glyph, kDim);

    RECT titleRect{ area.left, icon.bottom + Scale(state, 20), area.right, icon.bottom + Scale(state, 50) };
    DrawTextRect(dc, state.fontHeading, title, titleRect, kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT subtitleRect{ area.left + Scale(state, 40), titleRect.bottom,
                       area.right - Scale(state, 40), titleRect.bottom + Scale(state, 24) };
    DrawTextRect(dc, state.fontBody, subtitle, subtitleRect, kMuted,
                 DT_CENTER | DT_TOP | DT_WORDBREAK);

    if (action != ACT_NONE && !actionLabel.empty()) {
        int width = ButtonWidth(state, dc, actionLabel);
        RECT button{ (area.left + area.right) / 2 - width / 2,
                     subtitleRect.bottom + Scale(state, 18),
                     (area.left + area.right) / 2 + width / 2,
                     subtitleRect.bottom + Scale(state, 18) + Scale(state, 32) };
        DrawButton(state, dc, button, actionLabel, BUTTON_SECONDARY,
                   IsHovered(state, action, L""), true, action);
    }
}

std::wstring MetaLine(const AppInfo& app) {
    std::wstring meta;
    if (app.stars > 0) {
        meta += L"★ " + std::to_wstring(app.stars);
    }
    std::wstring license = app.license;
    if (license == L"GPL-3.0") {
        license = L"GPL v3";
    } else if (license == L"AGPL-3.0") {
        license = L"AGPL v3";
    } else if (license == L"LGPL-3.0") {
        license = L"LGPL v3";
    }
    if (!license.empty() && license != L"NOASSERTION") {
        if (!meta.empty()) {
            meta += L"   ·   ";
        }
        meta += license;
    }
    if (!app.release.publishedAt.empty()) {
        if (!meta.empty()) {
            meta += L"   ·   ";
        }
        meta += L"Обновлено " + util::FormatDate(app.release.publishedAt);
    }
    return meta;
}

void DrawCard(AppState& state,
              HDC dc,
              const RECT& card,
              const std::wstring& title,
              const std::wstring& version,
              const std::wstring& description,
              const std::wstring& meta,
              const std::wstring& statusText,
              COLORREF statusBackground,
              COLORREF statusColor,
              int percent,
              const std::vector<std::pair<std::wstring, Action>>& actions,
              Action primaryAction,
              const std::wstring& primaryArg,
              bool primaryEnabled,
              bool primaryDanger = false,
              bool primaryGhost = false) {
    bool hovered = state.mouseInside && PtInRect(&card, state.mouse);
    FillRoundRectColor(dc, card, Scale(state, 10), hovered ? kCardHover : kCard);
    FrameRoundRect(dc, card, Scale(state, 10), hovered ? kBorderHover : kBorder);

    int padding = Scale(state, 20);
    int top = card.top + Scale(state, 15);
    int right = card.right - padding;

    int badgeY = top + Scale(state, 4);

    int statusWidth = MeasureText(dc, state.fontBadge, statusText) + Scale(state, 20);
    RECT statusRect{ right - statusWidth, badgeY, right, badgeY + Scale(state, 22) };
    FillRoundRectColor(dc, statusRect, Scale(state, 5), statusBackground);
    DrawTextRect(dc, state.fontBadge, statusText, statusRect, statusColor,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int versionWidth = MeasureText(dc, state.fontBadge, version) + Scale(state, 20);
    int versionLeft = statusRect.left - Scale(state, 8) - versionWidth;
    RECT titleRect{ card.left + padding, top - Scale(state, 1),
                    versionLeft - Scale(state, 10), top + Scale(state, 26) };
    DrawTextRect(dc, state.fontHeading, title, titleRect, kText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int versionX = versionLeft;
    DrawBadge(state, dc, versionX, badgeY, version, kAccentSoft, RGB(160, 190, 255));

    RECT descriptionRect{ card.left + padding, top + Scale(state, 32),
                          right, top + Scale(state, 76) };
    DrawTextRect(dc, state.fontBody, description, descriptionRect, kMuted,
                 DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

    int buttonHeight = Scale(state, 32);
    int buttonY = card.bottom - Scale(state, 47);
    int buttonX = right;

    RECT primaryRect{};
    std::wstring primaryLabel = actions.empty() ? std::wstring() : actions.front().first;
    if (!primaryLabel.empty()) {
        int width = ButtonWidth(state, dc, primaryLabel);
        primaryRect = { buttonX - width, buttonY, buttonX, buttonY + buttonHeight };
        buttonX -= width + Scale(state, 10);
        ButtonStyle style = primaryDanger ? BUTTON_DANGER : (primaryGhost ? BUTTON_GHOST : BUTTON_PRIMARY);
        bool enabled = primaryEnabled;
        bool downloading = (state.installing && state.installingRepo == primaryArg && primaryAction == ACT_INSTALL) ||
                           (state.selfUpdating && primaryAction == ACT_SELF_UPDATE);
        if (downloading) {
            primaryLabel = state.installText.empty() ? L"Установка…" : state.installText;
            enabled = false;
        }
        DrawButton(state, dc, primaryRect, primaryLabel, style,
                   IsHovered(state, primaryAction, primaryArg), enabled, primaryAction, primaryArg);
    }

    for (size_t i = 1; i < actions.size(); ++i) {
        int width = ButtonWidth(state, dc, actions[i].first);
        RECT rect{ buttonX - width, buttonY, buttonX, buttonY + buttonHeight };
        buttonX -= width + Scale(state, 10);
        DrawButton(state, dc, rect, actions[i].first,
                   actions[i].second == ACT_UNINSTALL ? BUTTON_DANGER : BUTTON_SECONDARY,
                   IsHovered(state, actions[i].second, primaryArg), true,
                   actions[i].second, primaryArg);
    }

    RECT metaRect{ card.left + padding, card.bottom - Scale(state, 38),
                   (std::max)(static_cast<int>(card.left + padding + Scale(state, 40)), buttonX - Scale(state, 12)),
                   card.bottom - Scale(state, 20) };
    DrawTextRect(dc, state.fontSmall, meta, metaRect, kDim,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (percent >= 0) {
        int barWidth = (card.right - card.left) * percent / 100;
        RECT bar{ card.left, card.bottom - Scale(state, 3), card.left + barWidth, card.bottom };
        FillRectColor(dc, bar, kAccent);
    }
}

void DrawHeader(AppState& state, HDC dc, const RECT& client) {
    int height = Scale(state, 72);
    RECT header{ 0, 0, client.right, height };
    FillRectColor(dc, header, kHeader);
    Line(dc, 0, height - 1, client.right, height - 1, kBorder);

    int logoSize = Scale(state, 40);
    RECT logo{ Scale(state, 24), Scale(state, 16), 0, 0 };
    logo.right = logo.left + logoSize;
    logo.bottom = logo.top + logoSize;
    FillRoundRectColor(dc, logo, Scale(state, 10), kAccent);
    RECT glyph{ logo.left + logoSize / 4, logo.top + logoSize / 5,
                logo.right - logoSize / 4, logo.bottom - logoSize / 6 };
    DrawIconDownload(dc, glyph, kWhite);

    RECT brand{ Scale(state, 76), Scale(state, 10), client.right - Scale(state, 300), Scale(state, 40) };
    DrawTextRect(dc, state.fontTitle, L"BackupStore", brand, kText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitle{ Scale(state, 77), Scale(state, 41), client.right - Scale(state, 300), Scale(state, 62) };
    DrawTextRect(dc, state.fontSmall, L"Магазин приложений Backup Team", subtitle, kMuted,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int buttonHeight = Scale(state, 36);
    int buttonWidth = Scale(state, 132);
    RECT refresh{ client.right - Scale(state, 24) - buttonWidth, Scale(state, 18),
                  client.right - Scale(state, 24), Scale(state, 18) + buttonHeight };
    std::wstring refreshLabel = state.loadingCatalog ? L"Обновление…" : L"Обновить";
    DrawButton(state, dc, refresh, refreshLabel, BUTTON_SECONDARY,
               IsHovered(state, ACT_REFRESH, L""), !state.loadingCatalog, ACT_REFRESH);

    std::wstring version = state.storeUpdateAvailable ? (L"v" + std::wstring(kAppVersion) + L" · есть обновление")
                                                     : (L"v" + std::wstring(kAppVersion));
    int versionWidth = MeasureText(dc, state.fontSmall, version);
    RECT versionRect{ refresh.left - Scale(state, 16) - versionWidth, Scale(state, 24),
                      refresh.left - Scale(state, 16), Scale(state, 48) };
    DrawTextRect(dc, state.fontSmall, version, versionRect,
                 state.storeUpdateAvailable ? kOrange : kDim,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void DrawSidebar(AppState& state, HDC dc, const RECT& client) {
    int top = Scale(state, 72);
    int bottom = client.bottom - Scale(state, 30);
    int width = Scale(state, 220);
    RECT sidebar{ 0, top, width, bottom };
    FillRectColor(dc, sidebar, kSidebar);
    Line(dc, width - 1, top, width - 1, bottom, kBorder);

    struct NavItem {
        const wchar_t* label;
        View view;
        int badge;
    };
    NavItem items[4] = {
        { L"Каталог", VIEW_CATALOG, 0 },
        { L"Установлено", VIEW_INSTALLED, static_cast<int>(state.installed.size()) },
        { L"Обновления", VIEW_UPDATES, CountUpdates(state) },
        { L"Настройки", VIEW_SETTINGS, 0 }
    };

    int y = top + Scale(state, 20);
    int itemHeight = Scale(state, 44);
    for (int i = 0; i < 4; ++i) {
        RECT item{ Scale(state, 12), y, width - Scale(state, 12), y + itemHeight };
        bool selected = state.view == items[i].view;
        bool hovered = IsHovered(state, ACT_NAV, L"", static_cast<int>(items[i].view));
        if (selected) {
            FillRoundRectColor(dc, item, Scale(state, 8), RGB(25, 30, 42));
            RECT accent{ item.left, item.top + Scale(state, 10), item.left + Scale(state, 3), item.bottom - Scale(state, 10) };
            FillRoundRectColor(dc, accent, Scale(state, 2), kAccent);
        } else if (hovered) {
            FillRoundRectColor(dc, item, Scale(state, 8), RGB(19, 23, 32));
        }

        RECT icon{ item.left + Scale(state, 16), item.top + Scale(state, 14),
                   item.left + Scale(state, 32), item.top + Scale(state, 30) };
        COLORREF iconColor = selected ? kAccent : kMuted;
        switch (items[i].view) {
            case VIEW_CATALOG: DrawIconCatalog(dc, icon, iconColor); break;
            case VIEW_INSTALLED: DrawIconCheck(dc, icon, iconColor); break;
            case VIEW_UPDATES: DrawIconArrowUp(dc, icon, iconColor); break;
            case VIEW_SETTINGS: DrawIconSettings(dc, icon, iconColor); break;
        }

        RECT label{ item.left + Scale(state, 44), item.top, item.right - Scale(state, 44), item.bottom };
        DrawTextRect(dc, selected ? state.fontBodyBold : state.fontBody,
                     items[i].label, label, selected ? kText : kMuted,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        if (items[i].badge > 0) {
            std::wstring badgeText = std::to_wstring(items[i].badge);
            int badgeWidth = MeasureText(dc, state.fontBadge, badgeText) + Scale(state, 14);
            RECT badge{ item.right - Scale(state, 12) - badgeWidth, item.top + Scale(state, 13),
                        item.right - Scale(state, 12), item.top + Scale(state, 31) };
            COLORREF bg = items[i].view == VIEW_UPDATES ? kOrangeSoft : kButton;
            COLORREF fg = items[i].view == VIEW_UPDATES ? kOrange : kMuted;
            FillRoundRectColor(dc, badge, Scale(state, 9), bg);
            DrawTextRect(dc, state.fontBadge, badgeText, badge, fg,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        AddTarget(state, item, ACT_NAV, L"", static_cast<int>(items[i].view));
        y += itemHeight + Scale(state, 4);
    }

    int bottomY = bottom - Scale(state, 74);
    RECT github{ Scale(state, 24), bottomY, width - Scale(state, 20), bottomY + Scale(state, 24) };
    bool githubHover = IsHovered(state, ACT_GITHUB, L"");
    AddTarget(state, github, ACT_GITHUB, L"");
    DrawTextRect(dc, state.fontBody, L"GitHub", github, githubHover ? kText : kMuted,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT copyright{ Scale(state, 24), bottomY + Scale(state, 26), width - Scale(state, 20), bottomY + Scale(state, 48) };
    DrawTextRect(dc, state.fontSmall, L"© 2026 Backup Team", copyright, kDim,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void DrawStatusBar(AppState& state, HDC dc, const RECT& client) {
    int height = Scale(state, 30);
    RECT bar{ 0, client.bottom - height, client.right, client.bottom };
    FillRectColor(dc, bar, kHeader);
    Line(dc, 0, bar.top, client.right, bar.top, kBorder);

    RECT left{ Scale(state, 24), bar.top + 1, client.right - Scale(state, 280), bar.bottom };
    DrawTextRect(dc, state.fontSmall, state.status, left,
                 state.statusError ? kDanger : kMuted,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    std::wstring summary = std::to_wstring(state.apps.size()) + L" приложений   ·   " +
                           std::to_wstring(state.installed.size()) + L" установлено";
    RECT right{ client.right - Scale(state, 300), bar.top + 1, client.right - Scale(state, 24), bar.bottom };
    DrawTextRect(dc, state.fontSmall, summary, right, kDim,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    if ((state.installing || state.selfUpdating) && state.installPercent >= 0) {
        int width = (client.right - Scale(state, 220)) * state.installPercent / 100;
        RECT progress{ Scale(state, 220), client.bottom - Scale(state, 2),
                       Scale(state, 220) + width, client.bottom };
        FillRectColor(dc, progress, kAccent);
    }
}

std::vector<AppInfo> FilteredApps(const AppState& state) {
    std::vector<AppInfo> apps;
    for (const AppInfo& app : state.apps) {
        if (!state.search.empty()) {
            if (!util::ContainsI(app.repo, state.search) &&
                !util::ContainsI(app.description, state.search)) {
                continue;
            }
        }
        apps.push_back(app);
    }

    std::sort(apps.begin(), apps.end(), [&state](const AppInfo& left, const AppInfo& right) {
        auto leftInstalled = state.installed.find(left.repo);
        auto rightInstalled = state.installed.find(right.repo);
        bool leftUpdate = leftInstalled != state.installed.end() && HasUpdate(leftInstalled->second, left);
        bool rightUpdate = rightInstalled != state.installed.end() && HasUpdate(rightInstalled->second, right);
        if (leftUpdate != rightUpdate) {
            return leftUpdate;
        }
        return _wcsicmp(left.repo.c_str(), right.repo.c_str()) < 0;
    });
    return apps;
}

void DrawContentHeader(AppState& state, HDC dc, const RECT& client, int& dividerY) {
    int left = Scale(state, 220) + Scale(state, 26);
    int right = client.right - Scale(state, 26);
    int top = Scale(state, 72) + Scale(state, 24);

    std::wstring title;
    std::wstring subtitle;
    switch (state.view) {
        case VIEW_CATALOG:
            title = L"Каталог";
            subtitle = L"Приложения и утилиты Backup Team";
            break;
        case VIEW_INSTALLED:
            title = L"Установленные";
            subtitle = std::to_wstring(state.installed.size()) + L" приложений на этом компьютере";
            break;
        case VIEW_UPDATES:
            title = L"Обновления";
            subtitle = CountUpdates(state) > 0 ? L"Доступны новые версии"
                                               : L"Все установленные приложения обновлены";
            break;
        case VIEW_SETTINGS:
            title = L"Настройки";
            subtitle = L"Пути, обновления и информация";
            break;
    }

    RECT titleRect{ left, top, right - (state.view == VIEW_CATALOG ? Scale(state, 310) : 0), top + Scale(state, 30) };
    DrawTextRect(dc, state.fontTitle, title, titleRect, kText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT subtitleRect{ left, top + Scale(state, 32),
                       right - (state.view == VIEW_CATALOG ? Scale(state, 310) : 0), top + Scale(state, 54) };
    DrawTextRect(dc, state.fontSmall, subtitle, subtitleRect, kMuted,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (state.view == VIEW_CATALOG) {
        int searchWidth = Scale(state, 280);
        int searchHeight = Scale(state, 36);
        int searchTop = top + Scale(state, 3);
        RECT search{ right - searchWidth, searchTop, right, searchTop + searchHeight };
        state.searchRect = search;
        FillRoundRectColor(dc, search, Scale(state, 8), kCard);
        FrameRoundRect(dc, search, Scale(state, 8), state.searchFocused ? kAccent : kBorder);

        RECT icon{ search.left + Scale(state, 12), search.top + Scale(state, 10),
                   search.left + Scale(state, 26), search.top + Scale(state, 24) };
        DrawIconSearch(dc, icon, state.searchFocused ? kAccent : kDim);

        RECT textRect{ search.left + Scale(state, 36), search.top,
                       search.right - Scale(state, 12), search.bottom };
        if (state.search.empty() && !state.searchFocused) {
            DrawTextRect(dc, state.fontBody, L"Поиск приложений", textRect, kDim,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            DrawTextRect(dc, state.fontBody, state.search, textRect, kText,
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (state.searchFocused && state.caretVisible) {
                int textWidth = MeasureText(dc, state.fontBody, state.search);
                int caretX = (std::min)(textRect.left + textWidth + Scale(state, 1), textRect.right - Scale(state, 2));
                RECT caret{ caretX, search.top + Scale(state, 9), caretX + 1, search.bottom - Scale(state, 9) };
                FillRectColor(dc, caret, kAccent);
            }
        }
        AddTarget(state, search, ACT_SEARCH, L"");
    }

    dividerY = top + Scale(state, 66);
    Line(dc, left, dividerY, right, dividerY, kBorder);
}

void DrawCatalog(AppState& state, HDC dc, const RECT& client, int top, int bottom) {
    std::vector<AppInfo> apps = FilteredApps(state);
    int left = Scale(state, 220) + Scale(state, 26);
    int right = client.right - Scale(state, 26) - Scale(state, 10);
    int cardHeight = Scale(state, 148);
    int gap = Scale(state, 12);
    int total = static_cast<int>(apps.size()) * (cardHeight + gap) - (apps.empty() ? 0 : gap);
    state.contentHeight = (std::max)(total, bottom - top);
    int maxScroll = (std::max)(0, state.contentHeight - (bottom - top));
    if (state.scroll > maxScroll) {
        state.scroll = maxScroll;
    }

    if (apps.empty()) {
        DrawEmptyState(state, dc, RECT{ left, top, right, bottom },
                       state.search.empty() ? L"Каталог пока пуст" : L"Ничего не найдено",
                       state.search.empty() ? L"Опубликуйте релиз на GitHub — приложение появится здесь автоматически."
                                            : L"Попробуйте изменить поисковый запрос.");
        return;
    }

    int y = top - state.scroll;
    for (const AppInfo& app : apps) {
        RECT card{ left, y, right, y + cardHeight };
        if (card.bottom >= top && card.top <= bottom) {
            auto installed = state.installed.find(app.repo);
            bool isInstalled = installed != state.installed.end();
            bool updateAvailable = isInstalled && HasUpdate(installed->second, app);
            bool installing = state.installing && _wcsicmp(state.installingRepo.c_str(), app.repo.c_str()) == 0;
            int percent = installing ? state.installPercent : -1;

            std::wstring statusText;
            COLORREF statusBg = kButton;
            COLORREF statusFg = kMuted;
            if (installing) {
                statusText = L"Скачивание " + std::to_wstring(state.installPercent) + L"%";
                statusBg = kAccentSoft;
                statusFg = RGB(160, 190, 255);
            } else if (updateAvailable) {
                statusText = L"Доступно обновление";
                statusBg = kOrangeSoft;
                statusFg = kOrange;
            } else if (isInstalled) {
                statusText = L"Установлено";
                statusBg = kGreenSoft;
                statusFg = kGreen;
            } else if (app.archived) {
                statusText = L"Архив";
            } else if (!ChooseAsset(app.release)) {
                statusText = L"Нет файла";
            } else {
                statusText = L"Готово к установке";
                statusBg = kButton;
                statusFg = kMuted;
            }

            std::vector<std::pair<std::wstring, Action>> actions;
            Action primaryAction = ACT_INSTALL;
            bool primaryEnabled = true;
            std::wstring primaryLabel;
            bool danger = false;
            if (installing) {
                primaryLabel = state.installText.empty() ? L"Установка…" : state.installText;
                primaryEnabled = false;
                primaryAction = ACT_INSTALL;
                actions.push_back({ primaryLabel, ACT_INSTALL });
                actions.push_back({ L"Подробнее", ACT_RELEASE });
            } else if (isInstalled) {
                primaryLabel = updateAvailable ? L"Обновить" : L"Открыть";
                primaryAction = updateAvailable ? ACT_INSTALL : ACT_LAUNCH;
                primaryEnabled = updateAvailable || util::FileExists(installed->second.exe);
                actions.push_back({ primaryLabel, primaryAction });
                actions.push_back({ L"Открыть", ACT_LAUNCH });
                actions.push_back({ L"Папка", ACT_FOLDER });
                if (!updateAvailable) {
                    actions.erase(actions.begin() + 1);
                }
            } else {
                primaryLabel = L"Установить";
                primaryAction = ACT_INSTALL;
                primaryEnabled = ChooseAsset(app.release) != nullptr;
                actions.push_back({ primaryLabel, primaryAction });
                actions.push_back({ L"Подробнее", ACT_RELEASE });
            }

            std::wstring description = app.description;
            if (description.empty()) {
                description = L"Описание не указано.";
            }
            DrawCard(state, dc, card, app.repo, app.release.tag, description, MetaLine(app),
                     statusText, statusBg, statusFg, percent, actions,
                     primaryAction, app.repo, primaryEnabled, danger);
        }
        y += cardHeight + gap;
    }
    state.contentHeight = (std::max)(total + 8, bottom - top);
}

void DrawInstalled(AppState& state, HDC dc, const RECT& client, int top, int bottom) {
    int left = Scale(state, 220) + Scale(state, 26);
    int right = client.right - Scale(state, 26) - Scale(state, 10);
    int cardHeight = Scale(state, 148);
    int gap = Scale(state, 12);
    int count = static_cast<int>(state.installed.size());
    int total = count * (cardHeight + gap) - (count == 0 ? 0 : gap);
    state.contentHeight = (std::max)(total, bottom - top);
    int maxScroll = (std::max)(0, state.contentHeight - (bottom - top));
    if (state.scroll > maxScroll) {
        state.scroll = maxScroll;
    }

    if (state.installed.empty()) {
        DrawEmptyState(state, dc, RECT{ left, top, right, bottom },
                       L"Пока ничего не установлено",
                       L"Откройте каталог и установите первое приложение.");
        return;
    }

    int y = top - state.scroll;
    for (const auto& [repo, installed] : state.installed) {
        RECT card{ left, y, right, y + cardHeight };
        if (card.bottom >= top && card.top <= bottom) {
            const AppInfo* app = FindStoreApp(state, repo);
            bool update = app && HasUpdate(installed, *app);
            bool installing = state.installing && _wcsicmp(state.installingRepo.c_str(), repo.c_str()) == 0;
            int percent = installing ? state.installPercent : -1;

            std::wstring statusText = installing ? (L"Скачивание " + std::to_wstring(state.installPercent) + L"%")
                                                : (update ? L"Доступно обновление" : L"Установлено");
            COLORREF statusBg = installing ? kAccentSoft : (update ? kOrangeSoft : kGreenSoft);
            COLORREF statusFg = installing ? RGB(160, 190, 255) : (update ? kOrange : kGreen);

            std::vector<std::pair<std::wstring, Action>> actions;
            Action primary = ACT_LAUNCH;
            bool enabled = util::FileExists(installed.exe);
            if (installing) {
                actions.push_back({ state.installText.empty() ? L"Установка…" : state.installText, ACT_INSTALL });
                actions.push_back({ L"Папка", ACT_FOLDER });
                primary = ACT_INSTALL;
                enabled = false;
            } else if (update && app) {
                actions.push_back({ L"Обновить", ACT_INSTALL });
                actions.push_back({ L"Открыть", ACT_LAUNCH });
                actions.push_back({ L"Папка", ACT_FOLDER });
                actions.push_back({ L"Удалить", ACT_UNINSTALL });
                primary = ACT_INSTALL;
                enabled = true;
            } else {
                actions.push_back({ L"Открыть", ACT_LAUNCH });
                actions.push_back({ L"Папка", ACT_FOLDER });
                actions.push_back({ L"Удалить", ACT_UNINSTALL });
                primary = ACT_LAUNCH;
            }

            std::wstring description = installed.path;
            std::wstring meta = L"Версия " + installed.version;
            if (!installed.installedAt.empty()) {
                meta += L"   ·   Установлено " + util::FormatDate(installed.installedAt);
            }

            DrawCard(state, dc, card, installed.title.empty() ? repo : installed.title,
                     installed.version, description, meta, statusText, statusBg, statusFg,
                     percent, actions, primary, repo, enabled);
        }
        y += cardHeight + gap;
    }
    state.contentHeight = (std::max)(total + 8, bottom - top);
}

void DrawUpdates(AppState& state, HDC dc, const RECT& client, int top, int bottom) {
    int left = Scale(state, 220) + Scale(state, 26);
    int right = client.right - Scale(state, 26) - Scale(state, 10);
    int cardHeight = Scale(state, 148);
    int gap = Scale(state, 12);

    struct UpdateEntry {
        InstalledApp installed;
        const AppInfo* app = nullptr;
    };
    std::vector<UpdateEntry> entries;
    for (const auto& [repo, installed] : state.installed) {
        const AppInfo* app = FindStoreApp(state, repo);
        if (app && HasUpdate(installed, *app)) {
            entries.push_back({ installed, app });
        }
    }

    bool hasStoreUpdate = state.storeUpdateAvailable && state.storeRelease.has_value();
    int count = static_cast<int>(entries.size()) + (hasStoreUpdate ? 1 : 0);
    int total = count * (cardHeight + gap) - (count == 0 ? 0 : gap);
    state.contentHeight = (std::max)(total, bottom - top);
    int maxScroll = (std::max)(0, state.contentHeight - (bottom - top));
    if (state.scroll > maxScroll) {
        state.scroll = maxScroll;
    }

    if (count == 0) {
        if (state.checkingStoreUpdate) {
            DrawEmptyState(state, dc, RECT{ left, top, right, bottom },
                           L"Проверка обновлений…",
                           L"Запрашиваем данные с GitHub.");
        } else {
            DrawEmptyState(state, dc, RECT{ left, top, right, bottom },
                           L"Всё обновлено",
                           L"Новых версий установленных приложений нет.",
                           ACT_CHECK_STORE_UPDATE, L"Проверить снова");
        }
        return;
    }

    int y = top - state.scroll;
    if (hasStoreUpdate) {
        RECT card{ left, y, right, y + cardHeight };
        if (card.bottom >= top && card.top <= bottom) {
            std::vector<std::pair<std::wstring, Action>> actions = {
                { L"Обновить магазин", ACT_SELF_UPDATE },
                { L"Подробнее", ACT_RELEASE }
            };
            DrawCard(state, dc, card, L"BackupStore", state.storeRelease->tag,
                     L"Доступно обновление магазина приложений Backup Team.",
                     L"Текущая версия v" + std::wstring(kAppVersion), L"Обновление магазина",
                     kAccentSoft, RGB(160, 190, 255), state.selfUpdating ? state.installPercent : -1,
                     actions, ACT_SELF_UPDATE, L"", !state.selfUpdating);
        }
        y += cardHeight + gap;
    }

    for (const UpdateEntry& entry : entries) {
        RECT card{ left, y, right, y + cardHeight };
        if (card.bottom >= top && card.top <= bottom) {
            bool installing = state.installing && _wcsicmp(state.installingRepo.c_str(), entry.installed.repo.c_str()) == 0;
            std::vector<std::pair<std::wstring, Action>> actions = {
                { L"Обновить", ACT_INSTALL },
                { L"Открыть", ACT_LAUNCH },
                { L"Папка", ACT_FOLDER }
            };
            std::wstring description = entry.app->description.empty() ? entry.installed.path : entry.app->description;
            DrawCard(state, dc, card, entry.installed.repo, entry.app->release.tag,
                     description,
                     L"Установлено " + entry.installed.version + L"   ·   Новая версия " + entry.app->release.tag,
                     installing ? (L"Скачивание " + std::to_wstring(state.installPercent) + L"%") : L"Доступно обновление",
                     installing ? kAccentSoft : kOrangeSoft,
                     installing ? RGB(160, 190, 255) : kOrange,
                     installing ? state.installPercent : -1,
                     actions, ACT_INSTALL, entry.installed.repo, !installing && !state.installing);
        }
        y += cardHeight + gap;
    }
    state.contentHeight = (std::max)(total + 8, bottom - top);
}

void DrawSectionLabel(AppState& state, HDC dc, int x, int y, int width, const wchar_t* text) {
    RECT rect{ x, y, x + width, y + Scale(state, 20) };
    DrawTextRect(dc, state.fontBadge, text, rect, kDim,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void DrawSettingsRow(AppState& state,
                     HDC dc,
                     const RECT& row,
                     const std::wstring& label,
                     const std::wstring& value,
                     const std::wstring& buttonLabel,
                     Action action,
                     bool primary = false,
                     bool enabled = true) {
    int padding = Scale(state, 18);
    RECT labelRect{ row.left + padding, row.top, row.left + Scale(state, 210), row.bottom };
    DrawTextRect(dc, state.fontBodyBold, label, labelRect, kText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int buttonWidth = buttonLabel.empty() ? 0 : ButtonWidth(state, dc, buttonLabel);
    RECT valueRect{ row.left + Scale(state, 210), row.top,
                    row.right - padding - (buttonWidth > 0 ? buttonWidth + Scale(state, 16) : 0), row.bottom };
    DrawTextRect(dc, state.fontBody, value, valueRect, kMuted,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (!buttonLabel.empty()) {
        RECT button{ row.right - padding - buttonWidth, row.top + Scale(state, 10),
                     row.right - padding, row.bottom - Scale(state, 10) };
        DrawButton(state, dc, button, buttonLabel,
                   primary ? BUTTON_PRIMARY : BUTTON_SECONDARY,
                   IsHovered(state, action, L""), enabled, action);
    }
}

void DrawSettings(AppState& state, HDC dc, const RECT& client, int top, int bottom) {
    int left = Scale(state, 220) + Scale(state, 26);
    int right = client.right - Scale(state, 26) - Scale(state, 10);
    int vy = top;
    int y = vy - state.scroll;

    DrawSectionLabel(state, dc, left, y, right - left, L"ХРАНИЛИЩЕ");
    vy += Scale(state, 24);
    y = vy - state.scroll;

    RECT panel1{ left, y, right, y + Scale(state, 112) };
    FillRoundRectColor(dc, panel1, Scale(state, 10), kCard);
    FrameRoundRect(dc, panel1, Scale(state, 10), kBorder);
    RECT row1{ panel1.left, panel1.top, panel1.right, panel1.top + Scale(state, 56) };
    RECT row2{ panel1.left, panel1.top + Scale(state, 56), panel1.right, panel1.bottom };
    DrawSettingsRow(state, dc, row1, L"Папка установки", util::AppsDir(), L"Открыть", ACT_OPEN_APPS);
    DrawSettingsRow(state, dc, row2, L"Папка загрузок", util::DownloadsDir(), L"Открыть", ACT_OPEN_DOWNLOADS);
    Line(dc, panel1.left + Scale(state, 18), row2.top, panel1.right - Scale(state, 18), row2.top, kBorder);

    vy += Scale(state, 112) + Scale(state, 26);
    y = vy - state.scroll;
    DrawSectionLabel(state, dc, left, y, right - left, L"ОБНОВЛЕНИЯ");
    vy += Scale(state, 24);
    y = vy - state.scroll;

    RECT panel2{ left, y, right, y + Scale(state, 112) };
    FillRoundRectColor(dc, panel2, Scale(state, 10), kCard);
    FrameRoundRect(dc, panel2, Scale(state, 10), kBorder);
    RECT row3{ panel2.left, panel2.top, panel2.right, panel2.top + Scale(state, 56) };
    RECT row4{ panel2.left, panel2.top + Scale(state, 56), panel2.right, panel2.bottom };
    std::wstring versionValue = L"v" + std::wstring(kAppVersion);
    if (state.storeUpdateAvailable && state.storeRelease) {
        versionValue += L"  ·  доступна " + state.storeRelease->tag;
    }
    std::wstring updateButton = state.storeUpdateAvailable ? L"Обновить" : L"Проверить";
    DrawSettingsRow(state, dc, row3, L"Версия BackupStore", versionValue, updateButton,
                    state.storeUpdateAvailable ? ACT_SELF_UPDATE : ACT_CHECK_STORE_UPDATE,
                    state.storeUpdateAvailable,
                    !state.checkingStoreUpdate && !state.selfUpdating);
    std::wstring catalogValue = state.catalog.generatedAt.empty()
                                    ? L"Ещё не загружался"
                                    : L"Обновлён " + util::FormatDate(state.catalog.generatedAt) +
                                          (state.catalog.generatedAt.size() >= 16
                                               ? L" в " + state.catalog.generatedAt.substr(11, 5)
                                               : L"");
    DrawSettingsRow(state, dc, row4, L"Каталог приложений", catalogValue, L"Обновить", ACT_REFRESH,
                    false, !state.loadingCatalog);
    Line(dc, panel2.left + Scale(state, 18), row4.top, panel2.right - Scale(state, 18), row4.top, kBorder);

    vy += Scale(state, 112) + Scale(state, 26);
    y = vy - state.scroll;
    DrawSectionLabel(state, dc, left, y, right - left, L"О ПРОГРАММЕ");
    vy += Scale(state, 24);
    y = vy - state.scroll;

    RECT panel3{ left, y, right, y + Scale(state, 146) };
    FillRoundRectColor(dc, panel3, Scale(state, 10), kCard);
    FrameRoundRect(dc, panel3, Scale(state, 10), kBorder);
    RECT aboutTitle{ panel3.left + Scale(state, 18), panel3.top + Scale(state, 14),
                     panel3.right - Scale(state, 18), panel3.top + Scale(state, 38) };
    DrawTextRect(dc, state.fontBodyBold, L"BackupStore", aboutTitle, kText,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT aboutText{ panel3.left + Scale(state, 18), panel3.top + Scale(state, 42),
                    panel3.right - Scale(state, 18), panel3.top + Scale(state, 118) };
    DrawTextRect(dc, state.fontBody,
                 L"Открытый магазин приложений Backup Team. Программы загружаются напрямую из GitHub Releases, "
                 L"распаковываются и запускаются локально. Под лицензией GPL v3.",
                 aboutText, kMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);

    RECT licenseRow{ panel3.left, panel3.bottom - Scale(state, 38), panel3.right, panel3.bottom };
    RECT licenseText{ panel3.left + Scale(state, 18), licenseRow.top, panel3.right - Scale(state, 180), licenseRow.bottom };
    DrawTextRect(dc, state.fontSmall, L"© 2026 Backup Team · GPL v3", licenseText, kDim,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int buttonWidth = ButtonWidth(state, dc, L"Лицензия");
    RECT licenseButton{ panel3.right - Scale(state, 18) - buttonWidth, licenseRow.top + Scale(state, 3),
                        panel3.right - Scale(state, 18), licenseRow.bottom - Scale(state, 3) };
    DrawButton(state, dc, licenseButton, L"Лицензия", BUTTON_SECONDARY,
               IsHovered(state, ACT_LICENSE, L""), true, ACT_LICENSE);

    vy += Scale(state, 146) + Scale(state, 20);
    state.contentHeight = (std::max)(vy - top, bottom - top);
    int maxScroll = (std::max)(0, state.contentHeight - (bottom - top));
    if (state.scroll > maxScroll) {
        state.scroll = maxScroll;
    }
}

void DrawScrollbar(AppState& state, HDC dc, int top, int bottom, int left) {
    int viewport = bottom - top;
    if (state.contentHeight <= viewport || viewport <= 0) {
        return;
    }
    int trackWidth = Scale(state, 4);
    int x = left - trackWidth;
    RECT track{ x, top, x + trackWidth, bottom };
    FillRoundRectColor(dc, track, trackWidth / 2, kBorder);

    int maxScroll = state.contentHeight - viewport;
    int thumbHeight = (std::max)(Scale(state, 36), viewport * viewport / state.contentHeight);
    int thumbTop = top + (viewport - thumbHeight) * state.scroll / (maxScroll > 0 ? maxScroll : 1);
    RECT thumb{ x, thumbTop, x + trackWidth, thumbTop + thumbHeight };
    FillRoundRectColor(dc, thumb, trackWidth / 2, RGB(70, 80, 100));
}

} // namespace

std::wstring TargetKey(const HitTarget& target) {
    return MakeKey(target.action, target.arg, target.param);
}

bool HitTest(const AppState& state, POINT point, HitTarget& target) {
    for (auto it = state.targets.rbegin(); it != state.targets.rend(); ++it) {
        if (PtInRect(&it->rect, point)) {
            target = *it;
            return true;
        }
    }
    return false;
}

const AppInfo* FindStoreApp(const AppState& state, const std::wstring& repo) {
    return FindApp(state.apps, repo);
}

int CountCatalogUpdates(const AppState& state) {
    int count = 0;
    for (const auto& [repo, installed] : state.installed) {
        const AppInfo* app = FindApp(state.apps, repo);
        if (app && HasUpdate(installed, *app)) {
            ++count;
        }
    }
    return count;
}

int CountUpdates(const AppState& state) {
    return CountCatalogUpdates(state) + (state.storeUpdateAvailable ? 1 : 0);
}

std::wstring VersionBadgeText(const AppInfo& app) {
    return app.release.tag;
}

bool IsInstalled(const AppState& state, const std::wstring& repo) {
    return state.installed.find(repo) != state.installed.end();
}

void CreateUiFonts(AppState& state) {
    DestroyUiFonts(state);
    auto makeFont = [&state](int points, int weight) {
        return CreateFontW(-MulDiv(points, static_cast<int>(state.dpi), 72), 0, 0, 0, weight,
                           FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                           L"Segoe UI");
    };
    state.fontTitle = makeFont(22, FW_SEMIBOLD);
    state.fontHeading = makeFont(16, FW_BOLD);
    state.fontBody = makeFont(13, FW_NORMAL);
    state.fontBodyBold = makeFont(13, FW_SEMIBOLD);
    state.fontSmall = makeFont(11, FW_NORMAL);
    state.fontBadge = makeFont(10, FW_SEMIBOLD);
}

void DestroyUiFonts(AppState& state) {
    if (state.fontTitle) { DeleteObject(state.fontTitle); state.fontTitle = nullptr; }
    if (state.fontHeading) { DeleteObject(state.fontHeading); state.fontHeading = nullptr; }
    if (state.fontBody) { DeleteObject(state.fontBody); state.fontBody = nullptr; }
    if (state.fontBodyBold) { DeleteObject(state.fontBodyBold); state.fontBodyBold = nullptr; }
    if (state.fontSmall) { DeleteObject(state.fontSmall); state.fontSmall = nullptr; }
    if (state.fontBadge) { DeleteObject(state.fontBadge); state.fontBadge = nullptr; }
}

void PaintUi(AppState& state, HDC dc, const RECT& client) {
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);

    FillRectColor(memory, client, kBg);

    state.targets.clear();
    DrawHeader(state, memory, client);
    DrawSidebar(state, memory, client);

    int dividerY = 0;
    DrawContentHeader(state, memory, client, dividerY);
    int contentTop = dividerY + Scale(state, 16);
    int contentBottom = height - Scale(state, 30) - Scale(state, 12);
    state.contentRect = RECT{ Scale(state, 220) + Scale(state, 26), contentTop,
                              width - Scale(state, 26) - Scale(state, 10), contentBottom };

    switch (state.view) {
        case VIEW_CATALOG:
            DrawCatalog(state, memory, client, contentTop, contentBottom);
            break;
        case VIEW_INSTALLED:
            DrawInstalled(state, memory, client, contentTop, contentBottom);
            break;
        case VIEW_UPDATES:
            DrawUpdates(state, memory, client, contentTop, contentBottom);
            break;
        case VIEW_SETTINGS:
            DrawSettings(state, memory, client, contentTop, contentBottom);
            break;
    }

    DrawScrollbar(state, memory, contentTop, contentBottom, width - Scale(state, 26));
    DrawStatusBar(state, memory, client);

    BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
}
