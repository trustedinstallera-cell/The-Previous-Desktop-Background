/* Recommended configuration :
* Visual Studio with working set for Windows XP
* Microsoft Visual Studio .NET 2003 for Windows XP/2000
* Use following command if compiling with GCC:
*    g++ -o WallpaperChanger.exe wallpaper.cpp -lgdi32 -lole32 -lshell32 -lshlwapi -loleaut32 -luuid
* Windows 98 does not support native compilation. To run on Windows 98, compile
	  with Microsoft Visual Studio .NET 2003 on Windows 2000 or Windows XP
* No limitations for Windows Vista and above.
*/

#ifdef _WIN98_SUPPORT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <Wininet.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <olectl.h>
#include <shlwapi.h>
// Windows SDK v6.0 (November 2006) - Windows Vista
#if defined (NTDDI_VERSION) && NTDDI_VERSION > 0x06010000
#include <ShlObj_core.h>
#endif
#include "WindowsVersion.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")


#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// 获取当前 exe 所在目录
std::string GetExeDirectory() {
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string exePath(buffer);
	size_t pos = exePath.find_last_of("\\/");
	return exePath.substr(0, pos);
}

// 宽字符转普通字符串
std::string to_string(const std::wstring& wstr) {
	if (wstr.empty()) return "";
	int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

// 普通字符串转宽字符
std::wstring to_wstring(const std::string& str) {
	if (str.empty()) return L"";
	int wideLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	std::wstring wideStr(wideLen, L'\0');
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideLen);
	return wideStr;
}

bool ConvertJpgToBmp(const char* jpgPath, const char* bmpPath)
{
	HRESULT hr = CoInitialize(NULL);

	HANDLE hFile = CreateFileA(
		jpgPath,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		CoUninitialize();
		return false;
	}

	DWORD fileSize = GetFileSize(hFile, NULL);

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, fileSize);

	if (!hGlobal)
	{
		CloseHandle(hFile);
		CoUninitialize();
		return false;
	}

	void* pData = GlobalLock(hGlobal);

	DWORD bytesRead = 0;

	(void)ReadFile(hFile, pData, fileSize, &bytesRead, NULL); // check when hr fails
	GlobalUnlock(hGlobal);
	CloseHandle(hFile);
	IStream* pStream = NULL;
	hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pStream);

	if (FAILED(hr))
	{
		GlobalFree(hGlobal);
		CoUninitialize();
		return false;
	}

	IPicture* pPicture = NULL;

	hr = OleLoadPicture(pStream, fileSize, FALSE, IID_IPicture, (void**)&pPicture);

	if (FAILED(hr))
	{
		pStream->Release();
		CoUninitialize();
		return false;
	}

	//------------------------------------------------------------------
	// 获取图片尺寸
	//------------------------------------------------------------------

	OLE_XSIZE_HIMETRIC hmWidth;
	OLE_YSIZE_HIMETRIC hmHeight;

	pPicture->get_Width(&hmWidth);
	pPicture->get_Height(&hmHeight);

	int width = MulDiv(hmWidth, 96, 2540);
	int height = MulDiv(hmHeight, 96, 2540);

	if (width <= 0 || height <= 0)
	{
		pPicture->Release();
		pStream->Release();
		CoUninitialize();
		return false;
	}

	//------------------------------------------------------------------
	// 创建24位DIB
	//------------------------------------------------------------------

	BITMAPINFO bmi;

	ZeroMemory(&bmi, sizeof(bmi));

	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;

	// top-down DIB
	bmi.bmiHeader.biHeight = -height;

	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;

	LONG stride =
		((width * 24 + 31) / 32) * 4;

	DWORD imageSize =
		stride * height;

	HDC hdc = CreateCompatibleDC(NULL);

	void* pBits = NULL;

	HBITMAP hBitmap =
		CreateDIBSection(
			hdc,
			&bmi,
			DIB_RGB_COLORS,
			&pBits,
			NULL,
			0);

	if (!hBitmap)
	{
		DeleteDC(hdc);

		pPicture->Release();
		pStream->Release();

		CoUninitialize();

		return false;
	}

	HBITMAP hOldBmp =
		(HBITMAP)SelectObject(
			hdc,
			hBitmap);

	//------------------------------------------------------------------
	// 填充白底
	//------------------------------------------------------------------

	RECT rc;

	rc.left = 0;
	rc.top = 0;
	rc.right = width;
	rc.bottom = height;

	HBRUSH hBrush =
		(HBRUSH)GetStockObject(WHITE_BRUSH);

	FillRect(
		hdc,
		&rc,
		hBrush);

	//------------------------------------------------------------------
	// 绘制JPEG
	//------------------------------------------------------------------

	hr = pPicture->Render(
		hdc,
		0,
		0,
		width,
		height,
		0,
		hmHeight,
		hmWidth,
		-hmHeight,
		NULL);

	if (FAILED(hr))
	{
		SelectObject(hdc, hOldBmp);
		DeleteObject(hBitmap);
		DeleteDC(hdc);

		pPicture->Release();
		pStream->Release();

		CoUninitialize();

		return false;
	}

	//------------------------------------------------------------------
	// 保存BMP
	//------------------------------------------------------------------

	HANDLE hBmpFile =
		CreateFileA(
			bmpPath,
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

	if (hBmpFile == INVALID_HANDLE_VALUE)
	{
		SelectObject(hdc, hOldBmp);
		DeleteObject(hBitmap);
		DeleteDC(hdc);

		pPicture->Release();
		pStream->Release();

		CoUninitialize();

		return false;
	}

	BITMAPFILEHEADER bfh;

	ZeroMemory(&bfh, sizeof(bfh));

	bfh.bfType = 0x4D42;

	bfh.bfOffBits =
		sizeof(BITMAPFILEHEADER) +
		sizeof(BITMAPINFOHEADER);

	bfh.bfSize =
		bfh.bfOffBits +
		imageSize;

	BITMAPINFOHEADER bih;

	ZeroMemory(&bih, sizeof(bih));

	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = width;

	// BMP必须Bottom-Up
	bih.biHeight = height;

	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = imageSize;

	DWORD written;

	WriteFile(
		hBmpFile,
		&bfh,
		sizeof(bfh),
		&written,
		NULL);

	WriteFile(
		hBmpFile,
		&bih,
		sizeof(bih),
		&written,
		NULL);

	//------------------------------------------------------------------
	// Top-Down转Bottom-Up
	//------------------------------------------------------------------

	BYTE* rowBuffer = new BYTE[stride];

	BYTE* bits = (BYTE*)pBits;

	int y;

	for (y = height - 1; y >= 0; --y)
	{
		WriteFile(
			hBmpFile,
			bits + y * stride,
			stride,
			&written,
			NULL);
	}

	delete[] rowBuffer;

	CloseHandle(hBmpFile);

	//------------------------------------------------------------------
	// 清理
	//------------------------------------------------------------------

	SelectObject(
		hdc,
		hOldBmp);

	DeleteObject(
		hBitmap);

	DeleteDC(
		hdc);

	pPicture->Release();
	pStream->Release();

	CoUninitialize();

	return true;
}

// 设置壁纸

#if (defined _WIN32_WINNT && _WIN32_WINNT>=0x0502) || (defined NTDDI_VERSION && NTDDI_VERSION>=0x05020001)
bool SetWallpaperWinXP(const std::wstring& imagePath) {
	// 检查文件是否存在
	if (GetFileAttributesW(imagePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
		std::cout << "[错误] 文件不存在: " << to_string(imagePath) << std::endl;
		return false;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr)) {
		std::cout << "[错误] COM 初始化失败" << std::endl;
		return false;
	}

	IActiveDesktop* pActiveDesktop = NULL;
	hr = CoCreateInstance(CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER,
		IID_IActiveDesktop, (void**)&pActiveDesktop);

	bool success = false;
	if (SUCCEEDED(hr)) {
		hr = pActiveDesktop->SetWallpaper(imagePath.c_str(), 0);

		if (SUCCEEDED(hr)) {
			hr = pActiveDesktop->ApplyChanges(AD_APPLY_ALL);
			if (SUCCEEDED(hr)) {
				std::cout << "[成功] 壁纸已切换: " << to_string(imagePath) << std::endl;
				success = true;
			}
		}

		pActiveDesktop->Release();
	}

	CoUninitialize();
	return success;
}
#else
bool SetWallpaperWin98(const char* imagePath) {
	// 检查文件是否存在
	if (GetFileAttributesA(imagePath) == INVALID_FILE_ATTRIBUTES) {
		return false;
	}

	char bmpPath[MAX_PATH];
	bool isBmp = false;
	char tempPath[MAX_PATH];

	// 检查扩展名
	const char* ext = strrchr(imagePath, '.');
	if (ext && (_stricmp(ext, ".bmp") == 0)) {
		strcpy(bmpPath, imagePath);
		isBmp = true;
	}
	else {
		// 获取临时路径
		GetTempPathA(MAX_PATH, tempPath);

		// 创建临时 BMP 文件名
		static int counter = 0;
		sprintf(bmpPath, "%swallpaper_%d.bmp", tempPath, counter++);

		// 转换图片
		if (!ConvertJpgToBmp(imagePath, bmpPath)) {
			return false;
		}
	}

	// 设置壁纸
	BOOL result = SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0,
		(PVOID)bmpPath,
		SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);

	// 如果不是 BMP 文件，延迟删除临时文件
	if (!isBmp) {
		// Windows 98 下使用批处理延迟删除
		char delCmd[MAX_PATH + 50];
		sprintf(delCmd, "cmd.exe /c del \"%s\"", bmpPath);

		// 使用 ShellExecute 异步执行删除
		ShellExecuteA(NULL, "open", "cmd.exe",
			(std::string("/c del \"") + bmpPath + "\"").c_str(),
			NULL, SW_HIDE);
	}

	return result == TRUE;
}
#endif

bool SetWallpaper(std::string path) {

#if (defined _WIN32_WINNT && _WIN32_WINNT>=0x0502) || (defined NTDDI_VERSION && NTDDI_VERSION>=0x05020001)
	// Windows XP SP2 and above
	return SetWallpaperWinXP(to_wstring(path));
#else
	// Windows 98, Windows 2000, Windows XP RTM and Windows XP SP1
	return SetWallpaperWin98(path.c_str());
#endif
}


// 获取 SendTo 文件夹路径
std::string GetSendToPath() {
	char path[MAX_PATH];

#if (_WIN32_WINNT >= 0x0500)  // Windows 2000及以上
	if (SHGetFolderPathA(NULL, CSIDL_SENDTO, NULL, 0, path) == S_OK) {
		return std::string(path);
	}
#else  // Windows 98/ME
	// Windows 98使用SHGetSpecialFolderPathA
	if (SHGetSpecialFolderPathA(NULL, path, CSIDL_SENDTO, FALSE)) {
		return std::string(path);
	}
#endif

	return "";
}

// 替换 VBS 命令中的 {} 占位符为程序路径
void replaceVBSCommand(std::string& str) {
	char buffer[MAX_PATH];
	// 获取当前进程的可执行文件完整路径
	DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);

	size_t pos = str.find("{}");
	if (pos != std::string::npos) {
		str.replace(pos, 2, std::string(buffer, length));
	}
}

// 注册右键菜单（XP 风格，使用 SendTo）
void RegisterContextMenuWindowsXP() {
	// 模板字符串：{} 在引号内，replaceVBSCommand 只替换 {} 为路径
	std::string nextText = "CreateObject(\"WScript.Shell\").Run \"\"\"{}\"\" /next\", 0, False";
	replaceVBSCommand(nextText);

	std::string previousText = "CreateObject(\"WScript.Shell\").Run \"\"\"{}\"\" /previous\", 0, False";
	replaceVBSCommand(previousText);

	std::string randomText = "CreateObject(\"WScript.Shell\").Run \"\"\"{}\"\" /random\", 0, False";
	replaceVBSCommand(randomText);

	// 获取 SendTo 路径并创建 VBS 文件
	std::string sendToPath = GetSendToPath();
	if (sendToPath.empty()) {
		std::cerr << "[错误] 无法获取 SendTo 文件夹路径" << std::endl;
		return;
	}

	// 创建 下一张桌面背景.vbs
	std::ofstream nextFile((sendToPath + "\\下一张桌面背景.vbs").c_str());
	if (nextFile.is_open()) {
		nextFile << nextText;
		nextFile.close();
	}
	else {
		std::cerr << "[错误] 无法创建文件：下一张桌面背景.vbs" << std::endl;
		return;
	}

	// 创建 上一张桌面背景.vbs
	std::ofstream previousFile((sendToPath + "\\上一张桌面背景.vbs").c_str());
	if (previousFile.is_open()) {
		previousFile << previousText;
		previousFile.close();
	}
	else {
		std::cerr << "[错误] 无法创建文件：上一张桌面背景.vbs" << std::endl;
		return;
	}

	// 创建 随机桌面背景.vbs
	std::ofstream randomFile((sendToPath + "\\随机桌面背景.vbs").c_str());
	if (randomFile.is_open()) {
		randomFile << randomText;
		randomFile.close();
	}
	else {
		std::cerr << "[错误] 无法创建文件：随机桌面背景.vbs" << std::endl;
		return;
	}

	// 通知系统文件关联已更改
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 右键菜单已注册" << std::endl;
	std::cout << "[提示] 请在任意文件的“发送到”菜单中执行操作" << std::endl;
}

// 读取配置文件中的历史记录
std::vector<std::string> LoadHistory(const std::string& configPath) {
	std::vector<std::string> history;

	int count = GetPrivateProfileIntA("History", "Count", 0, configPath.c_str());

	for (int i = 0; i < count; ++i) {
		char key[16];
		char buffer[MAX_PATH];
		sprintf(key, "%d", i);

		if (GetPrivateProfileStringA("History", key, "", buffer, MAX_PATH, configPath.c_str())) {
			if (strlen(buffer) > 0) {
				history.push_back(std::string(buffer));
			}
		}
	}

	return history;
}

// 保存历史记录
void SaveHistory(const std::string& configPath, const std::vector<std::string>& history) {
	// 限制最大 20 条
	std::vector<std::string> trimmed = history;
	while (trimmed.size() > 20) {
		trimmed.erase(trimmed.begin());
	}

	char countBuf[16];
	sprintf(countBuf, "%d", (int)trimmed.size());
	WritePrivateProfileStringA("History", "Count", countBuf, configPath.c_str());

	for (size_t i = 0; i < trimmed.size(); ++i) {
		char keyBuf[16];
		sprintf(keyBuf, "%d", (int)i);
		WritePrivateProfileStringA("History", keyBuf, trimmed[i].c_str(), configPath.c_str());
	}
}

// 添加当前壁纸到历史
void AddToHistory(const std::string& configPath, const std::string& wallpaper) {
	std::vector<std::string> history = LoadHistory(configPath);

	// 避免重复添加同一张
	if (!history.empty() && history.back() == wallpaper) {
		return;
	}

	history.push_back(wallpaper);
	SaveHistory(configPath, history);
}

// 切换到上一个壁纸
bool SwitchToPrevious(const std::string& configPath) {
	std::vector<std::string> history = LoadHistory(configPath);

	if (history.size() < 2) {
		std::cout << "[提示] 历史记录不足，无法切换" << std::endl;
		return false;
	}

	// 获取上一个壁纸（倒数第二张）
	std::string prevWallpaper = history[history.size() - 2];

	if (SetWallpaper(prevWallpaper.c_str())) {
		// 更新历史：把当前壁纸移到最后
		std::string current = history.back();
		history.pop_back();
		history.push_back(current);
		SaveHistory(configPath, history);
		return true;
	}

	return false;
}

// 获取所有图片文件
std::vector<std::string> GetImageFiles(const std::string& folder) {
	std::vector<std::string> images;
	std::string searchPath = folder + "\\*.*";

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				std::string ext = findData.cFileName;
				// 转换为小写比较
				for (size_t j = 0; j < ext.size(); ++j) {
					ext[j] = tolower(ext[j]);
				}

				if (ext.find(".jpg") != std::string::npos ||
					ext.find(".jpeg") != std::string::npos ||
					ext.find(".png") != std::string::npos ||
					ext.find(".bmp") != std::string::npos) {
					images.push_back(folder + "\\" + findData.cFileName);
				}
			}
		} while (FindNextFileA(hFind, &findData));
		FindClose(hFind);
	}

	return images;
}

// 读取文件夹配置
std::vector<std::string> LoadFolders(const std::string& configPath) {
	std::vector<std::string> folders;

	int count = GetPrivateProfileIntA("Folders", "Count", 0, configPath.c_str());

	for (int i = 0; i < count; ++i) {
		char key[16];
		char buffer[MAX_PATH];
		sprintf(key, "%d", i);

		if (GetPrivateProfileStringA("Folders", key, "", buffer, MAX_PATH, configPath.c_str())) {
			if (strlen(buffer) > 0) {
				folders.push_back(std::string(buffer));
			}
		}
	}

	return folders;
}

// 获取所有图片（从所有配置的文件夹）
std::vector<std::string> GetAllImages(const std::string& configPath) {
	std::vector<std::string> allImages;
	std::vector<std::string> folders = LoadFolders(configPath);

	for (size_t i = 0; i < folders.size(); ++i) {
		const std::string& folder = folders[i];
		DWORD attr = GetFileAttributesA(folder.c_str());

		if (attr != INVALID_FILE_ATTRIBUTES) {
			if (attr & FILE_ATTRIBUTE_DIRECTORY) {
				// 是文件夹，扫描里面的图片
				std::vector<std::string> images = GetImageFiles(folder);
				for (size_t j = 0; j < images.size(); ++j) {
					allImages.push_back(images[j]);
				}
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
bool SwitchToNext(const std::string& configPath) {
	std::vector<std::string> allImages = GetAllImages(configPath);

	if (allImages.empty()) {
		int choice = MessageBoxA(NULL, "找不到任何图片。\n是否打开配置程序？", "信息", MB_YESNO);
		if (choice == IDYES) {
			std::string settingsPath = GetExeDirectory() + "\\Settings.exe";
			ShellExecuteA(NULL, "open", settingsPath.c_str(), NULL, NULL, SW_NORMAL);
		}
		std::cout << "[错误] 没有找到任何图片" << std::endl;
		return false;
	}

	// 获取上次使用的索引
	int lastIndex = GetPrivateProfileIntA("General", "LastIndex", 0, configPath.c_str());

	// 下一个索引（简单轮询）
	int nextIndex = (lastIndex + 1) % (int)allImages.size();
	std::string nextWallpaper = allImages[nextIndex];

	if (SetWallpaper(nextWallpaper.c_str())) {
		// 保存索引
		char indexBuf[16];
		sprintf(indexBuf, "%d", nextIndex);
		WritePrivateProfileStringA("General", "LastIndex", indexBuf, configPath.c_str());

		// 添加到历史
		AddToHistory(configPath, nextWallpaper);
		return true;
	}

	return false;
}

// 注册右键菜单（Windows 98 兼容版本）
void RegisterContextMenu() {
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	std::string exeFullPath = "\"" + std::string(exePath) + "\"";

	HKEY hKey;

	// 创建主菜单（在 Desktop 背景右键菜单中）
	std::string menuPath = "Directory\\Background\\Shell\\WallpaperChanger";

	if (RegCreateKeyExA(HKEY_CLASSES_ROOT, menuPath.c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

		// 设置菜单显示名称
		std::string menuText = "切换桌面背景(&W)";
		RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)menuText.c_str(),
			menuText.length() + 1);

		// 添加图标
		std::string icon = "shell32.dll, -154";
		RegSetValueExA(hKey, "Icon", 0, REG_SZ, (BYTE*)icon.c_str(),
			icon.length() + 1);

		// 设置菜单位置（显示在顶部）
		std::string position = "Top";
		RegSetValueExA(hKey, "Position", 0, REG_SZ, (BYTE*)position.c_str(),
			position.length() + 1);

		RegCloseKey(hKey);

		// 创建子菜单命令 - 上一个壁纸
		std::string cmdPath1 = menuPath + "\\command\\previous";
		if (RegCreateKeyExA(HKEY_CLASSES_ROOT, cmdPath1.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::string command = exeFullPath + " /previous";
			RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				command.length() + 1);
			RegCloseKey(hKey);
		}

		// 创建子菜单命令 - 下一个壁纸
		std::string cmdPath2 = menuPath + "\\command\\next";
		if (RegCreateKeyExA(HKEY_CLASSES_ROOT, cmdPath2.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::string command = exeFullPath + " /next";
			RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				command.length() + 1);
			RegCloseKey(hKey);
		}

		// 创建子菜单命令 - 随机壁纸
		// 并不需要真随机，忽略警告

		std::string cmdPath3 = menuPath + "\\command\\random";
		if (RegCreateKeyExA(HKEY_CLASSES_ROOT, cmdPath3.c_str(), 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

			std::string command = exeFullPath + " /random";
			RegSetValueExA(hKey, NULL, 0, REG_SZ, (BYTE*)command.c_str(),
				command.length() + 1);
			RegCloseKey(hKey);
		}
	}

	// 刷新右键菜单
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 右键菜单已注册" << std::endl;
	std::cout << "[提示] 请在桌面空白处右键查看" << std::endl;
}

// 取消注册右键菜单
void UnregisterContextMenu() {
	// 删除注册表项
	SHDeleteKeyA(HKEY_CLASSES_ROOT, "Directory\\Background\\Shell\\WallpaperChanger");

	// 刷新
	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

	std::cout << "[成功] 右键菜单已移除" << std::endl;
}

// 随机壁纸
bool SwitchToRandom(const std::string& configPath) {
	std::vector<std::string> allImages = GetAllImages(configPath);

	if (allImages.empty()) {
		int choice = MessageBoxA(NULL, "找不到任何图片。\n是否打开配置程序？", "信息", MB_YESNO);
		if (choice == IDYES) {
			std::string settingsPath = GetExeDirectory() + "\\Settings.exe";
			ShellExecuteA(NULL, "open", settingsPath.c_str(), NULL, NULL, SW_NORMAL);
		}
		std::cout << "[错误] 没有找到任何图片" << std::endl;
		return false;
	}

	// 随机选择
	srand(GetTickCount());
	int randomIndex = rand() % (int)allImages.size();
	std::string wallpaper = allImages[randomIndex];

	if (SetWallpaper(wallpaper.c_str())) {
		// 保存索引
		char indexBuf[16];
		sprintf(indexBuf, "%d", randomIndex);
		WritePrivateProfileStringA("General", "LastIndex", indexBuf, configPath.c_str());

		// 添加到历史
		AddToHistory(configPath, wallpaper);
		std::cout << "[提示] 已切换到随机壁纸" << std::endl;
		return true;
	}

	return false;
}

// 创建默认配置文件
void CreateDefaultConfig(const std::string& configPath) {
	// 检查配置文件是否存在
	if (GetFileAttributesA(configPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
		return;
	}

	std::cout << "[提示] 创建默认配置文件..." << std::endl;

	// 添加我的图片文件夹（如果存在）
	char myPicturesPath[MAX_PATH];
	if (SHGetSpecialFolderPathA(NULL, myPicturesPath, CSIDL_MYPICTURES, TRUE)) {
		std::string folder = myPicturesPath;
		WritePrivateProfileStringA("Folders", "0", folder.c_str(), configPath.c_str());
		WritePrivateProfileStringA("Folders", "Count", "1", configPath.c_str());
	}
	else {
		// 使用 Windows 目录作为备选
		char windowsPath[MAX_PATH];
		GetWindowsDirectoryA(windowsPath, MAX_PATH);
		std::string folder = std::string(windowsPath) + "\\Web\\Wallpaper";
		WritePrivateProfileStringA("Folders", "0", folder.c_str(), configPath.c_str());
		WritePrivateProfileStringA("Folders", "Count", "1", configPath.c_str());
	}

	// 初始化其他设置
	WritePrivateProfileStringA("General", "LastIndex", "0", configPath.c_str());
	WritePrivateProfileStringA("History", "Count", "0", configPath.c_str());
}

int main(int argc, char* argv[]) {
	// 获取配置文件路径
	std::string exeDir = GetExeDirectory();
	std::string configPath = exeDir + "\\config.ini";

	// 创建默认配置
	CreateDefaultConfig(configPath);

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
		else if (arg == "/random") {
			std::cout << "切换到随机壁纸..." << std::endl;
			SwitchToRandom(configPath);
			return 0;
		}
		else if (arg == "/register") {
			//RegisterContextMenu();
			RegisterContextMenuWindowsXP();
			return 0;
		}
		else if (arg == "/unregister") {
			UnregisterContextMenu();
			return 0;
		}
		else if (arg == "/set" && argc >= 3) {
			std::string imagePath(argv[2]);
			if (SetWallpaper(imagePath.c_str())) {
				AddToHistory(configPath, imagePath);
				std::cout << "[成功] 壁纸已设置" << std::endl;
			}
			else {
				std::cout << "[错误] 设置壁纸失败" << std::endl;
			}
			return 0;
		}
	}

	// 显示帮助
	std::cout << "命令说明：" << std::endl;
	std::cout << "  /previous       - 切换到上一个壁纸" << std::endl;
	std::cout << "  /next           - 切换到下一个壁纸" << std::endl;
	std::cout << "  /random         - 切换到随机壁纸" << std::endl;
	std::cout << "  /register       - 注册桌面右键菜单" << std::endl;
	std::cout << "  /unregister     - 移除右键菜单" << std::endl;
	std::cout << "  /set [路径]     - 设置指定壁纸" << std::endl;
	std::cout << "====================================" << std::endl;
	std::cout << "配置文件位置: " << configPath << std::endl;
	std::cout << "按任意键退出..." << std::endl;
	system("pause");

	return 0;
}

