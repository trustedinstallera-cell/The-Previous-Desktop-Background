// 完整的手动窗口创建版本（兼容 Windows 98）
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdio.h>
#include "Resource.h"
#include "WindowsVersion.h"

#if 1
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#endif

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// 控件 ID 定义
#define IDC_DATAGRID         1001
#define IDC_ADD_BUTTON       1002
#define IDC_INTERVAL_NUD     1003
#define IDC_SHUFFLE_CHECK    1004
#define IDC_PAUSE_NUD        1005
#define IDC_PADDING_COMBO    1006
#define IDC_OK_BUTTON        1007
#define IDC_CANCEL_BUTTON    1008
#define IDC_APPLY_BUTTON     1009
#define IDC_TIPS_STATIC      1010
#define IDC_INTERVAL_EDIT    1011
#define IDC_PAUSE_EDIT       1012
#define IDC_REMOVE_BUTTON    1013  // 新增：删除按钮

// 布局常量（Win98 兼容尺寸）
#define MARGIN_LEFT          10
#define MARGIN_RIGHT         10
#define MARGIN_TOP           10
#define MARGIN_BOTTOM        10
#define BUTTON_WIDTH         75
#define BUTTON_HEIGHT        23
#define EDIT_HEIGHT          20
#define LABEL_HEIGHT         16
#define ROW_SPACING          28
#define LISTVIEW_WIDTH       380
#define LISTVIEW_HEIGHT      180
#define RIGHT_PANEL_WIDTH    90

// 全局变量
HINSTANCE g_hInst = NULL;
HWND g_hMainWnd = NULL;
HWND g_hApplyButton = NULL;
HWND g_hIntervalEdit = NULL;
HWND g_hIntervalNud = NULL;
HWND g_hAddButton = NULL;
HWND g_hRemoveButton = NULL;
char g_configPath[MAX_PATH];
HFONT hFont = NULL;
float multiply = -1;

// 数据网格项
typedef struct {
    char name[MAX_PATH];
    BOOL isFile;
    BOOL isFolder;
    BOOL monitored;
    COLORREF bgColor;
} GridItem;

#define MAX_ITEMS 256
GridItem g_items[MAX_ITEMS];
int g_itemCount = 0;
HWND g_hListView = NULL;
HWND g_intervalText = NULL;
HIMAGELIST g_hImageList = NULL;

// 控件句柄数组（用于布局调整）
HWND g_hControls[20];
int g_nControlCount = 0;

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void LoadConfig(const char* path);
void SaveConfig(const char* path);
void RefreshDataGrid();
void AddItems();
void RemoveSelectedItem(void);  // 新增
void ValidateInterval(void);
void DoLayout(HWND hWnd, int cx, int cy);  // 新增：响应式布局
BOOL CreateControlsEx(HWND hWnd);  // 新增：带错误检查的控件创建

// 安全字符串复制（Win98 兼容）
static void SafeStrCpy(char* dest, const char* src, int destSize) {
    if (!dest || !src || destSize <= 0) return;
    int i;
    for (i = 0; i < destSize - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

// 安全字符串格式化（Win98 兼容）
static int SafeStrPrintf(char* dest, int destSize, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = wvsprintf(dest, fmt, args);  // Win98 可用
    va_end(args);
    if (destSize > 0 && result >= destSize) {
        dest[destSize - 1] = '\0';
        return destSize - 1;
    }
    return result;
}

// 加载配置
void LoadConfig(const char* path) {
    int count = GetPrivateProfileInt("Folders", "Count", 0, path);

    g_itemCount = 0;
    for (int i = 0; i < count && i < MAX_ITEMS; i++) {
        char key[16];
        SafeStrPrintf(key, sizeof(key), "%d", i);
        GetPrivateProfileString("Folders", key, "", g_items[g_itemCount].name, MAX_PATH, path);

        DWORD attrs = GetFileAttributes(g_items[g_itemCount].name);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            g_items[g_itemCount].bgColor = RGB(255, 200, 200);
            g_items[g_itemCount].isFile = FALSE;
            g_items[g_itemCount].isFolder = FALSE;
        }
        else {
            g_items[g_itemCount].bgColor = RGB(255, 255, 255);
            g_items[g_itemCount].isFolder = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            g_items[g_itemCount].isFile = !g_items[g_itemCount].isFolder;
        }

        g_items[g_itemCount].monitored = TRUE;
        g_itemCount++;
    }

    int interval = GetPrivateProfileInt("General", "Interval", 600, path);
    SetDlgItemInt(g_hMainWnd, IDC_INTERVAL_EDIT, interval, FALSE);

    int shuffle = GetPrivateProfileInt("General", "Shuffle", 0, path);
    CheckDlgButton(g_hMainWnd, IDC_SHUFFLE_CHECK, shuffle ? BST_CHECKED : BST_UNCHECKED);

    int pauseLimit = GetPrivateProfileInt("General", "PauseBatteryLimit", 0, path);
    SetDlgItemInt(g_hMainWnd, IDC_PAUSE_EDIT, pauseLimit, FALSE);
}

// 保存配置
void SaveConfig(const char* path) {
    char buffer[32];
    SafeStrPrintf(buffer, sizeof(buffer), "%d", g_itemCount);
    WritePrivateProfileString("Folders", "Count", buffer, path);

    for (int i = 0; i < g_itemCount; i++) {
        char key[16];
        SafeStrPrintf(key, sizeof(key), "%d", i);
        WritePrivateProfileString("Folders", key, g_items[i].name, path);
    }

    int interval = GetDlgItemInt(g_hMainWnd, IDC_INTERVAL_EDIT, NULL, FALSE);
    SafeStrPrintf(buffer, sizeof(buffer), "%d", interval);
    WritePrivateProfileString("General", "Interval", buffer, path);

    int shuffle = IsDlgButtonChecked(g_hMainWnd, IDC_SHUFFLE_CHECK) == BST_CHECKED;
    SafeStrPrintf(buffer, sizeof(buffer), "%d", shuffle);
    WritePrivateProfileString("General", "Shuffle", buffer, path);

    int pauseLimit = GetDlgItemInt(g_hMainWnd, IDC_PAUSE_EDIT, NULL, FALSE);
    SafeStrPrintf(buffer, sizeof(buffer), "%d", pauseLimit);
    WritePrivateProfileString("General", "PauseBatteryLimit", buffer, path);
}

// 刷新数据网格
void RefreshDataGrid() {
    if (!g_hListView) return;

    ListView_DeleteAllItems(g_hListView);

    // 重建图像列表
    if (g_hImageList) {
        ImageList_Destroy(g_hImageList);
    }
    g_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 10);

    SHFILEINFO shfi;

    for (int i = 0; i < g_itemCount; i++) {
        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
        lvItem.iItem = i;
        lvItem.pszText = g_items[i].name;
        lvItem.lParam = (LPARAM)i;  // 存储索引而不是指针，更安全

        // 获取图标
        if (g_items[i].isFolder) {
            SHGetFileInfo(g_items[i].name, FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi),
                SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
            lvItem.iImage = ImageList_AddIcon(g_hImageList, shfi.hIcon);
            DestroyIcon(shfi.hIcon);
        }
        else if (g_items[i].isFile) {
            SHGetFileInfo(g_items[i].name, 0, &shfi, sizeof(shfi),
                SHGFI_ICON | SHGFI_SMALLICON);
            lvItem.iImage = ImageList_AddIcon(g_hImageList, shfi.hIcon);
            DestroyIcon(shfi.hIcon);
        }
        else {
            // 不存在的文件，使用默认图标（索引 0）
            lvItem.iImage = 0;
        }

        ListView_InsertItem(g_hListView, &lvItem);
        ListView_SetCheckState(g_hListView, i, g_items[i].monitored);
    }

    ListView_SetImageList(g_hListView, g_hImageList, LVSIL_SMALL);

    // 修正：使用自定义绘制实现行背景色（Win98 兼容）
    // 注意：Win98 不完全支持 LVM_SETBKCOLOR  per-item，这里设置整体背景
    ListView_SetBkColor(g_hListView, RGB(255, 255, 255));
    ListView_SetTextBkColor(g_hListView, RGB(255, 255, 255));

    InvalidateRect(g_hListView, NULL, TRUE);
}

// 添加文件/文件夹
void AddItems() {
    OPENFILENAME ofn = { 0 };
    char szFile[4096] = { 0 };  // 加大缓冲区以支持多选
    char szFileTitle[260] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = "图片文件\0*.jpg;*.bmp;*.png;*.gif\0所有文件\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFileTitle = szFileTitle;
    ofn.nMaxFileTitle = sizeof(szFileTitle);
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_HIDEREADONLY;

    if (GetOpenFileNameA(&ofn)) {
        // 解析多选文件（Windows 98 兼容方式）
        char* p = szFile;
        char dir[MAX_PATH];
        SafeStrCpy(dir, p, sizeof(dir));
        p += lstrlen(p) + 1;

        if (*p == 0) {
            // 只选了一个文件
            if (g_itemCount < MAX_ITEMS) {
                SafeStrCpy(g_items[g_itemCount].name, dir, MAX_PATH);
                g_items[g_itemCount].isFile = TRUE;
                g_items[g_itemCount].isFolder = FALSE;
                g_items[g_itemCount].monitored = TRUE;
                g_items[g_itemCount].bgColor = RGB(255, 255, 255);
                g_itemCount++;
            }
        }
        else {
            // 选了多个文件
            while (*p && g_itemCount < MAX_ITEMS) {
                char fullPath[MAX_PATH];
                SafeStrPrintf(fullPath, sizeof(fullPath), "%s\\%s", dir, p);
                SafeStrCpy(g_items[g_itemCount].name, fullPath, MAX_PATH);
                g_items[g_itemCount].isFile = TRUE;
                g_items[g_itemCount].isFolder = FALSE;
                g_items[g_itemCount].monitored = TRUE;
                g_items[g_itemCount].bgColor = RGB(255, 255, 255);
                g_itemCount++;
                p += lstrlen(p) + 1;
            }
        }

        RefreshDataGrid();
        EnableWindow(g_hApplyButton, TRUE);
    }
}

// 新增：删除选中项
void RemoveSelectedItem(void) {
    if (!g_hListView) return;

    int selected = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (selected < 0 || selected >= g_itemCount) return;

    // 移动后续项目
    for (int i = selected; i < g_itemCount - 1; i++) {
        g_items[i] = g_items[i + 1];
    }
    g_itemCount--;

    RefreshDataGrid();
    EnableWindow(g_hApplyButton, TRUE);
}

// 验证间隔输入
void ValidateInterval() {
    char text[32];
    HWND hEdit = GetDlgItem(g_hMainWnd, IDC_INTERVAL_EDIT);
    HWND hTip = GetDlgItem(g_hMainWnd, IDC_TIPS_STATIC);

    GetWindowText(hEdit, text, sizeof(text));
    int value = atoi(text);

    if (value < 1) {  // 修正：最小值为 1，不是 0
        SetWindowText(hTip, "间隔至少为 1 秒");
        ShowWindow(hTip, SW_SHOW);
    }
    else if (value > 86400) {
        SetWindowText(hTip, "数值过大");
        ShowWindow(hTip, SW_SHOW);
    }
    else {
        ShowWindow(hTip, SW_HIDE);
        EnableWindow(g_hApplyButton, TRUE);
    }
}

// 检查电池
BOOL HasBattery() {
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        return status.BatteryFlag != 128;
    }
    return FALSE;
}

/**
 * 读取注册表DWORD值（REG_DWORD类型）
 *
 * @param hRootKey       注册表根键（如 HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER 等）
 * @param lpSubKey       子键路径（如 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion"）
 * @param lpValueName    值名称（如 "EnableLUA"）
 * @param dwDefaultValue 缺省值（可选，当键不存在或类型不匹配时返回此值，默认为0）
 * @return 读取到的DWORD值，或缺省值（如果读取失败）
 */
DWORD GetRegDWORDValue(
    HKEY    hRootKey,
    LPCSTR lpSubKey,
    LPCSTR lpValueName,
    DWORD   dwDefaultValue = 0
)
{
    HKEY   hKey = NULL;
    DWORD  dwValue = dwDefaultValue;
    DWORD  dwDataSize = sizeof(DWORD);
    DWORD  dwType = 0;
    LONG   lResult;

    // 打开注册表键
    lResult = RegOpenKeyExA(
        hRootKey,
        lpSubKey,
        0,
        KEY_QUERY_VALUE,
        &hKey
    );

    if (lResult != ERROR_SUCCESS)
    {
        // 键不存在或无权限访问，返回缺省值
        return dwDefaultValue;
    }

    // 读取值数据（自动检测类型）
    lResult = RegQueryValueExA(
        hKey,
        lpValueName,
        NULL,
        &dwType,
        reinterpret_cast<LPBYTE>(&dwValue),
        &dwDataSize
    );

    RegCloseKey(hKey);

    // 检查读取是否成功，且类型必须是 REG_DWORD 或 REG_DWORD_LITTLE_ENDIAN
    if (lResult != ERROR_SUCCESS || (dwType != REG_DWORD && dwType != REG_DWORD_LITTLE_ENDIAN))
    {
        return dwDefaultValue;
    }

    // 数据大小校验（确保是完整的DWORD）
    if (dwDataSize != sizeof(DWORD))
    {
        return dwDefaultValue;
    }

    return dwValue;
}

// 根据系统设置计算控件比值并返回最终值
// 当系统放大倍数改变时才重建
static float CalcControlSize(bool rebuild = false) {
    if (!rebuild && multiply != -1) return multiply;
    if (!(WindowsVersion.GetWindowsVersion() > WindowsVersion.Windows_98)) {
        return (float)GetRegDWORDValue(HKEY_CURRENT_CONFIG, "Display\\Settings", "DPILogicalX", 96) / 96.0f;
    }
    else {
        // 96 DPI == 100% scale
        // scale = DPI / 96 * 100%
        float i=GetRegDWORDValue(HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", "AppliedDPI", 96) / 96.0f;
        multiply = i;
        return i;
    }
}

// 新增：带错误检查的控件创建
static HWND CreateControl(DWORD dwExStyle, LPCTSTR lpClassName, LPCTSTR lpWindowName,
    DWORD dwStyle, int x, int y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {

    HWND hCtrl = CreateWindowExA(0L, lpClassName, lpWindowName, dwStyle, 1.25*x, 1.25 * y, CalcControlSize() * nWidth, CalcControlSize() * nHeight, hWndParent, hMenu, hInstance, lpParam);

    if (hCtrl && g_nControlCount < 20) {
        g_hControls[g_nControlCount++] = hCtrl;
    }

    return hCtrl;
}

// 新增：响应式布局
void DoLayout(HWND hWnd, int cx, int cy) {
    if (cx == 0 || cy == 0) return;  // 最小化时忽略

    // 计算可用区域
    int clientW = cx;
    int clientH = cy;

    // ListView 占据左侧大部分空间
    int lvW = clientW - MARGIN_LEFT - RIGHT_PANEL_WIDTH - MARGIN_RIGHT - 10;
    int lvH = LISTVIEW_HEIGHT;
    if (lvW < 200) lvW = 200;  // 最小宽度

    SetWindowPos(g_hListView, NULL,
        MARGIN_LEFT, MARGIN_TOP, lvW, lvH,
        SWP_NOZORDER | SWP_NOACTIVATE);

    // 右侧面板按钮
    int rightX = MARGIN_LEFT + lvW + 10;
    int btnY = MARGIN_TOP;

    HWND hAdd = GetDlgItem(hWnd, IDC_ADD_BUTTON);
    HWND hRemove = GetDlgItem(hWnd, IDC_REMOVE_BUTTON);

    if (hAdd) SetWindowPos(hAdd, NULL, rightX, btnY, RIGHT_PANEL_WIDTH, BUTTON_HEIGHT,
        SWP_NOZORDER | SWP_NOACTIVATE);
    if (hRemove) SetWindowPos(hRemove, NULL, rightX, btnY + BUTTON_HEIGHT + 5,
        RIGHT_PANEL_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

    // 重新设置列宽
    if (g_hListView) {
        int col0Width = lvW - 90;  // 留给复选框列 90px
        if (col0Width < 100) col0Width = 100;
        ListView_SetColumnWidth(g_hListView, 0, col0Width);
        ListView_SetColumnWidth(g_hListView, 1, 80);
    }

    // 下部区域
    int lowerY = MARGIN_TOP + lvH + 15;

    // 刷新间隔行
    HWND hIntervalLabel = GetDlgItem(hWnd, IDC_INTERVAL_EDIT);
    // 通过枚举子窗口找到静态文本（简化处理）

    // 底部按钮行（右对齐）
    int bottomY = clientH - MARGIN_BOTTOM - BUTTON_HEIGHT;
    int totalBtnWidth = 3 * BUTTON_WIDTH + 20;  // 3个按钮 + 间距
    int startX = clientW - MARGIN_RIGHT - totalBtnWidth;

    HWND hOK = GetDlgItem(hWnd, IDC_OK_BUTTON);
    HWND hCancel = GetDlgItem(hWnd, IDC_CANCEL_BUTTON);
    HWND hApply = GetDlgItem(hWnd, IDC_APPLY_BUTTON);

    if (hOK) SetWindowPos(hOK, NULL, startX, bottomY, BUTTON_WIDTH, BUTTON_HEIGHT,
        SWP_NOZORDER | SWP_NOACTIVATE);
    if (hCancel) SetWindowPos(hCancel, NULL, startX + BUTTON_WIDTH + 10, bottomY,
        BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);
    if (hApply) SetWindowPos(hApply, NULL, startX + 2 * (BUTTON_WIDTH + 10), bottomY,
        BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER | SWP_NOACTIVATE);

    // 其他控件位置调整...
    // 由于 Win98 限制，这里简化处理，实际应完整实现
}

// 创建所有控件（修正布局）
BOOL CreateControlsEx(HWND hWnd) {
    g_nControlCount = 0;

    int x = MARGIN_LEFT;
    int y = MARGIN_TOP;

    HFONT hFontParent = (HFONT)SendMessageA(hWnd, WM_GETFONT, 0, 0);

    // === 上部：ListView 和右侧按钮 ===
    // ListView (DataGrid)
    g_hListView = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, WC_LISTVIEW, "",
        WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        x, y, LISTVIEW_WIDTH, LISTVIEW_HEIGHT,
        hWnd, (HMENU)IDC_DATAGRID, g_hInst, NULL);
    if (!g_hListView) return FALSE;
    SendMessage(g_hListView, WM_SETFONT, (WPARAM)hFontParent, TRUE);

    // 添加列
    LVCOLUMN lvCol = { 0 };
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH;

    char colName1[] = "文件(夹)";
    lvCol.pszText = colName1;
    lvCol.cx = 290;  // 修正：给复选框列留足够空间
    ListView_InsertColumn(g_hListView, 0, &lvCol);

    char colName2[] = "监控";
    lvCol.pszText = colName2;
    lvCol.cx = 80;   // 修正：足够显示文字
    ListView_InsertColumn(g_hListView, 1, &lvCol);

    ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

    // 右侧按钮面板
    int rightX = x + LISTVIEW_WIDTH + 10;
    int rightY = y;

    // 添加按钮
    g_hAddButton = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "添加(&A)...",
        WS_CHILDWINDOW | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        rightX, rightY, RIGHT_PANEL_WIDTH, BUTTON_HEIGHT,
        hWnd, (HMENU)IDC_ADD_BUTTON, g_hInst, NULL);
    if (!g_hAddButton) return FALSE;
    SendMessage(g_hListView, WM_SETFONT, (WPARAM)hFontParent, TRUE);

    // 新增：删除按钮
    g_hRemoveButton = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "删除(&D)",
        WS_CHILDWINDOW | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        rightX, rightY + BUTTON_HEIGHT + 5, RIGHT_PANEL_WIDTH, BUTTON_HEIGHT,
        hWnd, (HMENU)IDC_REMOVE_BUTTON, g_hInst, NULL);
    if (!g_hRemoveButton) return FALSE;
    SendMessage(g_hRemoveButton, WM_SETFONT, (WPARAM)hFontParent, TRUE);

    // === 中部：设置区域 ===
    y += LISTVIEW_HEIGHT + 15;

    // 刷新间隔标签
    g_intervalText = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "STATIC", "刷新间隔(&I):",
        WS_CHILDWINDOW | WS_VISIBLE | SS_LEFT,
        x, y + 2, 80, LABEL_HEIGHT,
        hWnd, NULL, g_hInst, NULL);;
    if (!g_intervalText) return FALSE;

    // 刷新间隔编辑框（修正：增大宽度，避免文字截断）
    g_hIntervalEdit = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "EDIT", "600",
        WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
        x + 85, y, 50, EDIT_HEIGHT,
        hWnd, (HMENU)IDC_INTERVAL_EDIT, g_hInst, NULL);
    if (!g_hIntervalEdit) return FALSE;

    // 修正：UpDown 控件位置 - 紧跟 Edit 右侧
    g_hIntervalNud = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, UPDOWN_CLASS, NULL,
        WS_CHILDWINDOW | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_ALIGNRIGHT | WS_TABSTOP,
        x + 85 + 50, y, 16, EDIT_HEIGHT,  // 修正：宽度 16，紧贴 Edit
        hWnd, (HMENU)IDC_INTERVAL_NUD, g_hInst, NULL);
    if (!g_hIntervalNud) return FALSE;
    SendMessage(g_hIntervalNud, UDM_SETBUDDY, (WPARAM)g_hIntervalEdit, 0);
    SendMessage(g_hIntervalNud, UDM_SETRANGE, 0, MAKELPARAM(86400, 1));  // 设置范围

    // 修正：提示文本位置 - 放在 UpDown 右侧，不重叠
    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "STATIC", "",
        WS_CHILDWINDOW | WS_VISIBLE | SS_LEFT | SS_WORDELLIPSIS,
        x + 85 + 50 + 20, y + 2, 150, LABEL_HEIGHT,
        hWnd, (HMENU)IDC_TIPS_STATIC, g_hInst, NULL)) return FALSE;

    y += ROW_SPACING;

    // 随机播放复选框
    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "随机播放(&S)",
        WS_CHILDWINDOW | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, 120, EDIT_HEIGHT,
        hWnd, (HMENU)IDC_SHUFFLE_CHECK, g_hInst, NULL)) return FALSE;

    y += ROW_SPACING;

    // 电量限制标签
    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "STATIC", "低于指定电量时暂停(&B):",
        WS_CHILDWINDOW | WS_VISIBLE | SS_LEFT,
        x, y + 2, 160, LABEL_HEIGHT,
        hWnd, NULL, g_hInst, NULL)) return FALSE;

    // 电量编辑框
    HWND hPauseEdit = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "EDIT", "0",
        WS_CHILDWINDOW | WS_VISIBLE | WS_BORDER | ES_NUMBER | WS_TABSTOP,
        x + 165, y, 50, EDIT_HEIGHT,
        hWnd, (HMENU)IDC_PAUSE_EDIT, g_hInst, NULL);
    if (!hPauseEdit) return FALSE;

    // 修正：电量 UpDown 控件
    HWND hPauseNud = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, UPDOWN_CLASS, NULL,
        WS_CHILDWINDOW | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ARROWKEYS | UDS_ALIGNRIGHT | WS_TABSTOP,
        x + 165 + 50, y, 16, EDIT_HEIGHT,
        hWnd, (HMENU)IDC_PAUSE_NUD, g_hInst, NULL);
    if (!hPauseNud) return FALSE;
    SendMessage(hPauseNud, UDM_SETBUDDY, (WPARAM)hPauseEdit, 0);
    SendMessage(hPauseNud, UDM_SETRANGE, 0, MAKELPARAM(100, 0));  // 0-100%

    if (!HasBattery()) {
        EnableWindow(hPauseEdit, FALSE);
        EnableWindow(hPauseNud, FALSE);
    }

    y += ROW_SPACING;

    // 填充方式 ComboBox
    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "STATIC", "填充方式:",
        WS_CHILDWINDOW | WS_VISIBLE | SS_LEFT,
        x, y + 2, 60, LABEL_HEIGHT,
        hWnd, NULL, g_hInst, NULL)) return FALSE;

    HWND hCombo = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "COMBOBOX", "",
        WS_CHILDWINDOW | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
        x + 65, y, 100, 100,
        hWnd, (HMENU)IDC_PADDING_COMBO, g_hInst, NULL);
    if (!hCombo) return FALSE;

    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"居中");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"拉伸");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)"填充");
    SendMessage(hCombo, CB_SETCURSEL, 0, 0);

    // === 底部：操作按钮（右对齐）===
    y += ROW_SPACING + 10;
    int btnX = LISTVIEW_WIDTH + MARGIN_LEFT - 3 * (BUTTON_WIDTH + 10) + 80;

    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "确定(&O)",
        WS_CHILDWINDOW | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP | BS_DEFPUSHBUTTON,
        btnX, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hWnd, (HMENU)IDC_OK_BUTTON, g_hInst, NULL)) return FALSE;

    if (!CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "取消(&C)",
        WS_CHILDWINDOW | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        btnX + BUTTON_WIDTH + 10, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hWnd, (HMENU)IDC_CANCEL_BUTTON, g_hInst, NULL)) return FALSE;

    g_hApplyButton = CreateControl(WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR, "BUTTON", "应用(&A)",
        WS_CHILDWINDOW | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        btnX + 2 * (BUTTON_WIDTH + 10), y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hWnd, (HMENU)IDC_APPLY_BUTTON, g_hInst, NULL);
    if (!g_hApplyButton) return FALSE;
    EnableWindow(g_hApplyButton, FALSE);

    return TRUE;
}

BOOL CALLBACK EnumChildProc(HWND hChild, LPARAM lParam) {
    SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE); // TRUE表示立即重绘
    return TRUE;
}

// 窗口过程（包含完整的消息循环处理）
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        if (!CreateControlsEx(hWnd)) {
            MessageBox(hWnd, "创建控件失败", "错误", MB_OK | MB_ICONERROR);
            return -1;  // 终止窗口创建
        }

        EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hFont);

        GetModuleFileName(NULL, g_configPath, MAX_PATH);
        char* p = strrchr(g_configPath, '\\');
        if (p) *(p + 1) = 0;
        lstrcat(g_configPath, "config.ini");

        LoadConfig(g_configPath);
        RefreshDataGrid();
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ADD_BUTTON:
            AddItems();
            return 0;

        case IDC_REMOVE_BUTTON:  // 新增
            RemoveSelectedItem();
            return 0;

        case IDC_OK_BUTTON:
            SaveConfig(g_configPath);
            DestroyWindow(hWnd);
            return 0;

        case IDC_CANCEL_BUTTON:
            DestroyWindow(hWnd);
            return 0;

        case IDC_APPLY_BUTTON:
            SaveConfig(g_configPath);
            EnableWindow(g_hApplyButton, FALSE);
            return 0;

        case IDC_INTERVAL_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                ValidateInterval();
            }

        case IDC_SHUFFLE_CHECK:
        case IDC_PAUSE_EDIT:
        case IDC_PADDING_COMBO:
            EnableWindow(g_hApplyButton, TRUE);
            return 0;
        }
        break;

    case WM_NOTIFY: {
        NMHDR* pnmh = (NMHDR*)lParam;
        if (pnmh->idFrom == IDC_DATAGRID) {
            if (pnmh->code == NM_CLICK) {
                NMLISTVIEW* pnmv = (NMLISTVIEW*)lParam;
                if (pnmv->iItem >= 0) {
                    LVHITTESTINFO ht = { 0 };
                    ht.pt = pnmv->ptAction;
                    ListView_HitTest(g_hListView, &ht);
                    if (ht.flags & LVHT_ONITEMSTATEICON) {
                        g_items[pnmv->iItem].monitored = ListView_GetCheckState(g_hListView, pnmv->iItem);
                    }
                }
            }
            else if (pnmh->code == LVN_KEYDOWN) {
                // 支持 Delete 键删除
                NMLVKEYDOWN* pnmkd = (NMLVKEYDOWN*)lParam;
                if (pnmkd->wVKey == VK_DELETE) {
                    RemoveSelectedItem();
                }
            }
        }
        break;
    }

    case WM_SIZE:
        // 修正：实现响应式布局
        DoLayout(hWnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_GETMINMAXINFO: {
        // 修正：设置最小窗口尺寸
        MINMAXINFO* pMMI = (MINMAXINFO*)lParam;
        pMMI->ptMinTrackSize.x = 540;
        pMMI->ptMinTrackSize.y = 450;
        return 0;
    }

    case WM_DESTROY:
        if (g_hImageList) {
            ImageList_Destroy(g_hImageList);
            g_hImageList = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// WinMain - 包含完整的消息循环
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;


    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    // 初始化通用控件
    INITCOMMONCONTROLSEX icc = { 0 };
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icc);

    // 注册窗口类
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);  // 修正：使用 COLOR_BTNFACE 更协调
    wc.lpszClassName = "SettingWindowClass";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SETTINGS));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "注册窗口类失败", "错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    hFont = CreateFont(
        -12* CalcControlSize(),                    // 字体高度（负值表示字符高度）
        0,                      // 字体宽度（0=自动）
        0, 0,                   // 倾斜角度
        FW_NORMAL,              // 字重
        FALSE, FALSE, FALSE,    // 斜体、下划线、删除线
        DEFAULT_CHARSET,        // 字符集（关键！中文必须用这个）
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        "宋体"  // 或 "MS Shell Dlg"（Win98 兼容）
    );

    // 修正：窗口尺寸计算（考虑非客户区）
    RECT rc = { 0, 0, 540, 450 };  // 客户区尺寸
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    // 创建窗口
    g_hMainWnd = CreateWindowEx(WS_EX_CLIENTEDGE,  // 修正：添加客户端边框
        "SettingWindowClass", "设置程序",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,  // 修正：禁止随意调整大小（Win98 兼容）
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, "创建窗口失败", "错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    // ===== 完整的消息循环（Win98 兼容的加速键处理）=====
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // 先尝试对话框消息处理（Tab 键等）
        if (!IsDialogMessage(g_hMainWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    // =========================

    return msg.wParam;
}