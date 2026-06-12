#include <windows.h>

int main()
{
    LPCWSTR path = L"C:\\Test\\image.jpg";
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)path, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return 0;
}