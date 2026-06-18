#include <windows.h>
#include <vector>
#include <string>
#pragma comment(lib,"version.lib")

class CWindowsVersion {
public:
    enum {
        Undefined,
        Windows_95,
        Windows_NT_4_0,
        Windows_98,
        Windows_ME,
        Windows_2000,
        Windows_XP_RTM,
        Windows_XP_SP1,
        Windows_XP_SP2,
        Windows_XP_SP3,
        Windows_XP_X64,          // 实际是 Server 2003 (NT 5.2)
        Windows_Server_2003 = Windows_XP_X64,
        Windows_Vista_RTM,
        Windows_Server_2008 = Windows_Vista_RTM,
        Windows_Vista_SP1,
        Windows_Vista_SP2,
        Windows_7_RTM,
        Windows_Server_2008_R2 = Windows_7_RTM,
        Windows_7_SP1,
        Windows_8,
        Windows_8_RTM = Windows_8,
        Windows_8_1,
        Windows_Server_2012 = Windows_8_RTM,
        Windows_Server_2012_R2 = Windows_8_1,
        Windows_10_1507,
        Windows_10_1511,
        Windows_10_1607,
        Windows_10_1703,
        Windows_10_1709,
        Windows_10_1803,
        Windows_10_1809,
        Windows_10_1903,
        Windows_10_1909,
        Windows_10_2004,
        Windows_10_20H2,
        Windows_10_21H1,
        Windows_10_21H2,
        Windows_10_22H2,
        Windows_11_21H2,
        Windows_11_22H2,
        Windows_11_23H2,
        Windows_11_24H2
    };

    CWindowsVersion() {
        m_version = GetFileVersion();
    }

    int GetWindowsVersion() const {
        if (m_version.size() < 4) return Undefined;

        int major = m_version[0];
        int minor = m_version[1];
        int build = m_version[2];
        int revision = m_version[3];

        // 内核版本是 6.2 时，可能是 Win8、Win10 或 Win11
        if (major == 6 && minor == 2) {
            if (build >= 26100) return Windows_11_24H2;
            if (build >= 22631) return Windows_11_23H2;
            if (build >= 22621) return Windows_11_22H2;
            if (build >= 22000) return Windows_11_21H2;
            if (build >= 19045) return Windows_10_22H2;
            if (build >= 19044) return Windows_10_21H2;
            if (build >= 19043) return Windows_10_21H1;
            if (build >= 19042) return Windows_10_20H2;
            if (build >= 19041) return Windows_10_2004;
            if (build >= 18363) return Windows_10_1909;
            if (build >= 18362) return Windows_10_1903;
            if (build >= 17763) return Windows_10_1809;
            if (build >= 17134) return Windows_10_1803;
            if (build >= 16299) return Windows_10_1709;
            if (build >= 15063) return Windows_10_1703;
            if (build >= 14393) return Windows_10_1607;
            if (build >= 10586) return Windows_10_1511;
            if (build >= 10240) return Windows_10_1507;
            if (build >= 9200) return Windows_8_RTM;
        }

        // 内核版本是 6.3 时，是 Windows 8.1
        if (major == 6 && minor == 3 && build >= 9600) {
            return Windows_8_1;
        }

        // 内核版本是 6.1 时，是 Windows 7
        if (major == 6 && minor == 1) {
            if (build >= 7601) return Windows_7_SP1;
            if (build >= 7600) return Windows_7_RTM;
        }

        // 内核版本是 6.0 时，是 Windows Vista
        if (major == 6 && minor == 0) {
            if (build >= 6002) return Windows_Vista_SP2;
            if (build >= 6001 && revision >= 18000) return Windows_Vista_SP1;
            if (build >= 6000) return Windows_Vista_RTM;
        }

        // 内核版本是 5.2 时，是 XP x64 / Server 2003
        if (major == 5 && minor == 2 && build >= 3790) {
            return Windows_XP_X64;
        }

        // 内核版本是 5.1 时，是 Windows XP
        if (major == 5 && minor == 1 && build >= 2600) {
            if (revision >= 3300) return Windows_XP_SP3;
            if (revision >= 2180) return Windows_XP_SP2;
            if (revision >= 1106) return Windows_XP_SP1;
            return Windows_XP_RTM;
        }

        // 内核版本是 5.0 时，是 Windows 2000
        if (major == 5 && minor == 0 && build >= 2195) {
            return Windows_2000;
        }

        return Undefined;
    }

    // 获取友好名称（支持传入版本号，默认为当前系统）
    std::string GetFriendlyName(int ver = Undefined) const {
        if (ver == Undefined) {
            ver = GetWindowsVersion();
        }

        switch (ver) {
        case Windows_2000:          return "Windows 2000";
        case Windows_XP_RTM:        return "Windows XP RTM";
        case Windows_XP_SP1:        return "Windows XP SP1";
        case Windows_XP_SP2:        return "Windows XP SP2";
        case Windows_XP_SP3:        return "Windows XP SP3";
        case Windows_XP_X64:        return "Windows XP x64 / Server 2003";
        case Windows_Vista_RTM:     return "Windows Vista RTM";
        case Windows_Vista_SP1:     return "Windows Vista SP1";
        case Windows_Vista_SP2:     return "Windows Vista SP2";
        case Windows_7_RTM:         return "Windows 7 RTM";
        case Windows_7_SP1:         return "Windows 7 SP1";
        case Windows_8_RTM:         return "Windows 8";
        case Windows_8_1:           return "Windows 8.1";
        case Windows_10_1507:       return "Windows 10 1507 (RTM)";
        case Windows_10_1511:       return "Windows 10 1511 (November Update)";
        case Windows_10_1607:       return "Windows 10 1607 (Anniversary Update)";
        case Windows_10_1703:       return "Windows 10 1703 (Creators Update)";
        case Windows_10_1709:       return "Windows 10 1709 (Fall Creators Update)";
        case Windows_10_1803:       return "Windows 10 1803 (April 2018 Update)";
        case Windows_10_1809:       return "Windows 10 1809 (October 2018 Update)";
        case Windows_10_1903:       return "Windows 10 1903 (May 2019 Update)";
        case Windows_10_1909:       return "Windows 10 1909 (November 2019 Update)";
        case Windows_10_2004:       return "Windows 10 2004 (May 2020 Update)";
        case Windows_10_20H2:       return "Windows 10 20H2 (October 2020 Update)";
        case Windows_10_21H1:       return "Windows 10 21H1 (May 2021 Update)";
        case Windows_10_21H2:       return "Windows 10 21H2 (November 2021 Update)";
        case Windows_10_22H2:       return "Windows 10 22H2";
        case Windows_11_21H2:       return "Windows 11 21H2";
        case Windows_11_22H2:       return "Windows 11 22H2";
        case Windows_11_23H2:       return "Windows 11 23H2";
        case Windows_11_24H2:       return "Windows 11 24H2";
        default:                    return "Unknown Windows Version";
        }
    }

    // 获取详细版本信息（如 "Windows 11 24H2 (10.0.26100.8521)"）
    std::string GetDetailedInfo(int ver = Undefined) const {
        if (ver == Undefined) {
            ver = GetWindowsVersion();
        }

        std::string result = GetFriendlyName(ver);
        std::string verStr = GetVersionString();
        if (!verStr.empty()) {
            result += " (";
            result += verStr;
            result += ")";
        }
        return result;
    }

    // 获取版本号的字符串表示 (如 "6.2.26100.8521")
    std::string GetVersionString() const {
        if (m_version.size() < 4) return "";
        char buf[64];
        sprintf(buf, "%d.%d.%d.%d",
            m_version[0], m_version[1], m_version[2], m_version[3]);
        return std::string(buf);
    }

private:
    std::vector<int> m_version;

    std::vector<int> GetFileVersion() const {
        std::vector<int> result(4, 0);

        char systemPath[MAX_PATH];
        if (GetSystemDirectoryA(systemPath, MAX_PATH) == 0) {
            return result;
        }

        std::string filePath = std::string(systemPath) + "\\ntoskrnl.exe";

        DWORD dwSize = GetFileVersionInfoSizeA(filePath.c_str(), NULL);
        if (dwSize == 0) {
            return result;
        }

        BYTE* pVersionInfo = new BYTE[dwSize];
        if (!pVersionInfo) {
            return result;
        }

        if (!GetFileVersionInfoA(filePath.c_str(), 0, dwSize, pVersionInfo)) {
            delete[] pVersionInfo;
            return result;
        }

        VS_FIXEDFILEINFO* pFileInfo = NULL;
        UINT uLen = 0;
        if (!VerQueryValueA(pVersionInfo, "\\", (VOID**)&pFileInfo, &uLen)) {
            delete[] pVersionInfo;
            return result;
        }

        result[0] = HIWORD(pFileInfo->dwFileVersionMS);
        result[1] = LOWORD(pFileInfo->dwFileVersionMS);
        result[2] = HIWORD(pFileInfo->dwFileVersionLS);
        result[3] = LOWORD(pFileInfo->dwFileVersionLS);

        delete[] pVersionInfo;
        return result;
    }
} WindowsVersion;