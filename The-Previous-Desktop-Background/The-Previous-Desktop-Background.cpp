#include <windows.h>
#include <shlguid.h>
#include <Wininet.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <random>

// 宽字符转普通字符串
std::string WStringToString(const std::wstring& wstr) {
	if (wstr.empty()) return "";
	int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// 普通字符串转宽字符
std::wstring StringToWString(const std::string& str) {
	if (str.empty()) return L"";
	int wideLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	std::wstring wideStr(wideLen, L'\0');
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideLen);
	return wideStr;
}

// 获取当前 exe 所在目录
std::wstring GetExeDirectory() {
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);
	std::wstring exePath(buffer);
	size_t pos = exePath.find_last_of(L"\\/");
	return exePath.substr(0, pos);
}

// 设置壁纸（使用 IActiveDesktop）
bool SetWallpaper(const std::wstring& imagePath) {
	// 检查文件是否存在
	if (GetFileAttributesW(imagePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		std::cout << "[错误] 文件不存在: " << WStringToString(imagePath) << std::endl;
		return false;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
		std::cout << "[错误] COM 初始化失败" << std::endl;
		return false;
	}

	IActiveDesktop* pActiveDesktop = nullptr;
	hr = CoCreateInstance(CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER,
		IID_IActiveDesktop, (void**)&pActiveDesktop);

	bool success = false;
	if (SUCCEEDED(hr)) {
		hr = pActiveDesktop->SetWallpaper(imagePath.c_str(), 0);

		if (SUCCEEDED(hr)) {
			hr = pActiveDesktop->ApplyChanges(AD_APPLY_ALL);
			if (SUCCEEDED(hr)) {
				std::cout << "[成功] 壁纸已切换: " << WStringToString(imagePath) << std::endl;
				success = true;
			}
		}

		pActiveDesktop->Release();
	}

	CoUninitialize();
	return success;
}

// 读取配置文件中的历史记录
std::vector<std::wstring> LoadHistory(const std::wstring& configPath) {
	std::vector<std::wstring> history;

	int count = GetPrivateProfileIntW(L"History", L"Count", 0, configPath.c_str());

	for (int i = 0; i < count; i++) {
		wchar_t buffer[MAX_PATH];
		std::wstring key = std::to_wstring(i);

		if (GetPrivateProfileStringW(L"History", key.c_str(), L"", buffer, MAX_PATH, configPath.c_str())) {
			if (wcslen(buffer) > 0) {
				history.push_back(std::wstring(buffer));
			}
		}
	}

	return history;
}

// 保存历史记录
void SaveHistory(const std::wstring& configPath, const std::vector<std::wstring>& history) {
	// 限制最大 20 条
	std::vector<std::wstring> trimmed = history;
	while (trimmed.size() > 20) {
		trimmed.erase(trimmed.begin());
	}

	WritePrivateProfileStringW(L"History", L"Count", std::to_wstring(trimmed.size()).c_str(), configPath.c_str());

	for (size_t i = 0; i < trimmed.size(); i++) {
		std::wstring key = std::to_wstring(i);
		WritePrivateProfileStringW(L"History", key.c_str(), trimmed[i].c_str(), configPath.c_str());
	}
}

// 添加当前壁纸到历史
void AddToHistory(const std::wstring& configPath, const std::wstring& wallpaper) {
	auto history = LoadHistory(configPath);

	// 避免重复添加同一张
	if (!history.empty() && history.back() == wallpaper) {
		return;
	}

	history.push_back(wallpaper);
	SaveHistory(configPath, history);
}

// 切换到上一个壁纸
bool SwitchToPrevious(const std::wstring& configPath) {
	auto history = LoadHistory(configPath);

	if (history.size() < 2) {
		std::cout << "[提示] 历史记录不足，无法切换" << std::endl;
		return false;
	}

	// 获取上一个壁纸（倒数第二张）
	std::wstring prevWallpaper = history[history.size() - 2];

	if (SetWallpaper(prevWallpaper)) {
		// 更新历史：把当前壁纸移到最后
		std::wstring current = history.back();
		history.pop_back();
		history.push_back(current);
		SaveHistory(configPath, history);
		return true;
	}

	return false;
}

// 获取所有图片文件
std::vector<std::wstring> GetImageFiles(const std::wstring& folder) {
	std::vector<std::wstring> images;
	std::wstring searchPath = folder + L"\\*.*";

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				std::wstring ext = findData.cFileName;
				// 转换为小写比较
				for (auto& c : ext) c = towlower(c);

				if (ext.find(L".jpg") != std::wstring::npos ||
					ext.find(L".jpeg") != std::wstring::npos ||
					ext.find(L".png") != std::wstring::npos ||
					ext.find(L".bmp") != std::wstring::npos) {
					images.push_back(folder + L"\\" + findData.cFileName);
				}
			}
		} while (FindNextFileW(hFind, &findData));
		FindClose(hFind);
	}

	return images;
}

// 读取文件夹配置
std::vector<std::wstring> LoadFolders(const std::wstring& configPath) {
	std::vector<std::wstring> folders;

	int count = GetPrivateProfileIntW(L"Folders", L"Count", 0, configPath.c_str());

	for (int i = 0; i < count; i++) {
		wchar_t buffer[MAX_PATH];
		std::wstring key = std::to_wstring(i);

		if (GetPrivateProfileStringW(L"Folders", key.c_str(), L"", buffer, MAX_PATH, configPath.c_str())) {
			if (wcslen(buffer) > 0) {
				folders.push_back(std::wstring(buffer));
			}
		}
	}

	return folders;
}

// 获取所有图片（从所有配置的文件夹）
std::vector<std::wstring> GetAllImages(const std::wstring& configPath) {
	std::vector<std::wstring> allImages;
	auto folders = LoadFolders(configPath);

	for (const auto& folder : folders) {
		DWORD attr = GetFileAttributesW(folder.c_str());

		if (attr != INVALID_FILE_ATTRIBUTES) {
			if (attr & FILE_ATTRIBUTE_DIRECTORY) {
				// 是文件夹，扫描里面的图片
				auto images = GetImageFiles(folder);
				allImages.insert(allImages.end(), images.begin(), images.end());
			}
			else {
				// 是单个文件，直接添加
				allImages.push_back(folder);
			}
		}
	}

	return allImages;
}

// 切换到下一个壁纸
bool SwitchToNext(const std::wstring& configPath) {
	auto allImages = GetAllImages(configPath);

	if (allImages.empty()) {
		std::cout << "[错误] 没有找到任何图片" << std::endl;
		return false;
	}

	// 获取上次使用的索引
	int lastIndex = GetPrivateProfileIntW(L"General", L"LastIndex", 0, configPath.c_str());

	// 下一个索引（简单轮询）
	int nextIndex = (lastIndex + 1) % allImages.size();
	std::wstring nextWallpaper = allImages[nextIndex];

	if (SetWallpaper(nextWallpaper)) {
		// 保存索引
		WritePrivateProfileStringW(L"General", L"LastIndex", std::to_wstring(nextIndex).c_str(), configPath.c_str());

		// 添加到历史
		AddToHistory(configPath, nextWallpaper);
		return true;
	}

	return false;
}

// 注册桌面右键菜单（适用于 Windows 7/10/11）
void RegisterContextMenuWindows7() {
	// 获取当前 exe 的完整路径
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	std::wstring exeFullPath = L"\"" + std::wstring(exePath) + L"\"";

	// ========== 菜单1：上一个壁纸 ==========
	std::wstring keyPath1 = L"DesktopBackground\\Shell\\WallpaperPrevious";
	HKEY hKey;

	// 创建主键
	if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath1.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

		// 设置显示名称
		std::wstring menuText = L"上一个桌面背景";
		RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)menuText.c_str(),
			(menuText.length() + 1) * sizeof(wchar_t));

		// 可选：添加图标
		std::wstring icon = L"imageres.dll,-67";
		RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (BYTE*)icon.c_str(),
			(icon.length() + 1) * sizeof(wchar_t));

		RegCloseKey(hKey);

		// 创建 command 子键
		std::wstring commandPath1 = keyPath1 + L"\\command";
		if (RegCreateKeyExW(HKEY_CLASSES_ROOT, commandPath1.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::wstring command = exeFullPath + L" /previous";
			RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				(command.length() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}
	}

	// ========== 菜单2：下一个壁纸 ==========
	std::wstring keyPath2 = L"DesktopBackground\\Shell\\WallpaperNext";

	if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath2.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

		std::wstring menuText = L"下一个桌面背景";
		RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)menuText.c_str(),
			(menuText.length() + 1) * sizeof(wchar_t));

		std::wstring icon = L"imageres.dll,-67";
		RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (BYTE*)icon.c_str(),
			(icon.length() + 1) * sizeof(wchar_t));

		RegCloseKey(hKey);

		std::wstring commandPath2 = keyPath2 + L"\\command";
		if (RegCreateKeyExW(HKEY_CLASSES_ROOT, commandPath2.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::wstring command = exeFullPath + L" /next";
			RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				(command.length() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}
	}

	// 通知系统刷新右键菜单
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 右键菜单已注册到 DesktopBackground\\Shell" << std::endl;
	std::cout << "[提示] 请在桌面空白处右键查看菜单" << std::endl;
}

void RegisterContextMenuXP_NewMenu() {
	// 获取当前 exe 的完整路径
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	std::wstring exeFullPath = L"\"" + std::wstring(exePath) + L"\"";

	// XP 的“新建”菜单路径
	// HKEY_CLASSES_ROOT\Directory\Background\ShellNew
	std::wstring basePath = L"Directory\\Background\\ShellNew";
	HKEY hKey;

	// ========== 方式1：直接添加命令到 ShellNew（不推荐）==========
	// ShellNew 通常用于创建文件模板，不适合直接放命令

	// ========== 方式2：在“新建”菜单中创建子菜单（推荐尝试）==========
	// 实际上 XP 的“新建”菜单比较特殊，更好的方法是：
	// 在 Shell 下创建子菜单，然后设置 Extended 或 使用 SubCommands

	std::wstring menuPath = L"Directory\\Background\\Shell\\WallpaperChanger";

	if (RegCreateKeyExW(HKEY_CLASSES_ROOT, menuPath.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

		// 设置菜单显示名称（加 & 表示快捷键）
		std::wstring menuText = L"切换桌面背景(&W)";
		RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)menuText.c_str(),
			(menuText.length() + 1) * sizeof(wchar_t));

		// 添加图标
		std::wstring icon = L"shell32.dll,-154";  // 图片图标
		RegSetValueExW(hKey, L"Icon", 0, REG_SZ, (BYTE*)icon.c_str(),
			(icon.length() + 1) * sizeof(wchar_t));

		// 设置扩展属性（按住 Shift 才显示）
		// RegSetValueExW(hKey, L"Extended", 0, REG_SZ, (BYTE*)L"", 1);

		// 设置菜单位置
		std::wstring position = L"Top";
		RegSetValueExW(hKey, L"Position", 0, REG_SZ, (BYTE*)position.c_str(),
			(position.length() + 1) * sizeof(wchar_t));

		RegCloseKey(hKey);

		// 创建子菜单命令
		// 子键1：上一个壁纸
		std::wstring cmdPath1 = menuPath + L"\\command\\previous";
		if (RegCreateKeyExW(HKEY_CLASSES_ROOT, cmdPath1.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::wstring command = exeFullPath + L" /previous";
			RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				(command.length() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}

		// 子键2：下一个壁纸
		std::wstring cmdPath2 = menuPath + L"\\command\\next";
		if (RegCreateKeyExW(HKEY_CLASSES_ROOT, cmdPath2.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::wstring command = exeFullPath + L" /next";
			RegSetValueExW(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				(command.length() + 1) * sizeof(wchar_t));
			RegCloseKey(hKey);
		}
	}

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 菜单已添加到右键菜单（XP 模式）" << std::endl;
}

// 取消注册右键菜单
void UnregisterContextMenu() {
	std::wstring keyPath1 = L"DesktopBackground\\Shell\\WallpaperPrevious";
	std::wstring keyPath2 = L"DesktopBackground\\Shell\\WallpaperNext";

	// 删除整个键树
	RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath1.c_str());
	RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath2.c_str());

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 右键菜单已移除" << std::endl;
}

int main(int argc, char* argv[]) {
	// 获取配置文件路径（exe 所在目录下的 config.ini）
	std::wstring exeDir = GetExeDirectory();
	std::wstring configPath = exeDir + L"\\config.ini";

	// 解析命令行参数
	if (argc >= 2) {
		std::string arg(argv[1]);

		if (arg == "/previous") {
			std::cout << "切换到上一个壁纸..." << std::endl;
			SwitchToPrevious(configPath);
			return 0;
		}
		else if (arg == "/next") {
			std::cout << "切换到下一个壁纸..." << std::endl;
			SwitchToNext(configPath);
			return 0;
		}
		else if (arg == "/register") {
			RegisterContextMenuWindows7();
			return 0;
		}
		else if (arg == "/unregister") {
			UnregisterContextMenu();
			return 0;
		}
		else if (arg == "/set") {
			if (argc >= 3) {
				std::wstring imagePath = StringToWString(std::string(argv[2]));
				SetWallpaper(imagePath);
				AddToHistory(configPath, imagePath);
			}
			return 0;
		}
	}

	// 无参数或参数无效时，启动幻灯片模式
	std::cout << "=== 壁纸自动切换工具 ===" << std::endl;
	std::cout << "命令说明：" << std::endl;
	std::cout << "  /previous     - 切换到上一个壁纸" << std::endl;
	std::cout << "  /next         - 切换到下一个壁纸" << std::endl;
	std::cout << "  /register     - 注册桌面右键菜单" << std::endl;
	std::cout << "  /unregister   - 移除右键菜单" << std::endl;
	std::cout << "  /set [路径]   - 设置指定壁纸" << std::endl;
	std::cout << "  无参数        - 启动幻灯片模式" << std::endl;
	std::cout << "================================" << std::endl;
	system("pause");

	return 0;
}