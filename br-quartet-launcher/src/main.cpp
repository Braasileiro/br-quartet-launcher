#include <windows.h>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <objbase.h>

#include "resource.h"
#include "logger.hpp"
#include "version_info.h"

// Constants - General
constexpr LPCSTR LOG_FILENAME{ "br-quartet-launcher.log" };

// Constants - Window
constexpr LPCSTR WINDOW_NAME{ "BLUE REFLECTION Quartet" };
constexpr LPCSTR WINDOW_CLASS_NAME{ "DX11LauncherWndClass" };

// Constants - Options
constexpr LPCSTR OPT_QUARTET{ "quartet" };
constexpr LPCSTR OPT_BR{ "br" };
constexpr LPCSTR OPT_RAY{ "ray" };
constexpr LPCSTR OPT_SUN{ "sun" };
constexpr LPCSTR OPT_TIE{ "tie" };

// Constants - DLLs
constexpr LPCSTR DLL_QUARTET{ "B0.dll" };
constexpr LPCSTR DLL_BR{ "B1.dll" };
constexpr LPCSTR DLL_RAY{ "B2.dll" };
constexpr LPCSTR DLL_SUN{ "B3.dll" };
constexpr LPCSTR DLL_TIE{ "B4.dll" };

// Signatures
typedef void (*FuncDllInitialize)(HWND*);
typedef int (*FuncDllExecute)(void);
typedef LRESULT(CALLBACK* FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

// Global Exception Handler
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    std::ofstream crashLog(LOG_FILENAME, std::ios::app);

    if (crashLog.is_open()) {
        crashLog << "fatal: Crashed! Exception code: 0x"
            << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode
            << std::dec << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    // Setting the current working directory
    char exePathRaw[MAX_PATH];
    GetModuleFileNameA(NULL, exePathRaw, MAX_PATH);
    std::string exePath(exePathRaw);
    std::string::size_type pos = exePath.find_last_of("\\/");
    std::string currentDir = exePath.substr(0, pos);
    SetCurrentDirectoryA(currentDir.c_str());

    // Initialize global exception handler
    SetUnhandledExceptionFilter(CrashHandler);

    // COM initialize
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    Logger::Init(LOG_FILENAME, true);

    spdlog::info("=====================================");
    spdlog::info("br-quartet-launcher {} initialized", APP_PRODUCT_VERSION_A);
    spdlog::info("Command line: {}", GetCommandLineA());

    // Game mapping
    std::map<std::string, std::string> games = {
        {OPT_QUARTET, DLL_QUARTET},
        {OPT_BR,      DLL_BR},
        {OPT_RAY,     DLL_RAY},
        {OPT_SUN,     DLL_SUN},
        {OPT_TIE,     DLL_TIE}
    };

    std::string opt = OPT_QUARTET;

    // Handle initial argument for direct game boot
    if (__argc > 1) {
        opt = __argv[1];

        if (games.find(opt) == games.end()) {
            spdlog::warn("Invalid parameter '{}'. Falling back to '{}'", opt, OPT_QUARTET);

            opt = OPT_QUARTET;
        }
    }

    std::string dllName = games[opt];

    spdlog::info("Current target: {}", opt);
    spdlog::info("Loading '{}' into memory...", dllName);

    HMODULE hModule = LoadLibraryA(dllName.c_str());

    if (!hModule) {
        spdlog::error("Failed to load '{}'! Error: {}", dllName, GetLastError());
        return 1;
    }

    auto DllInitialize = (FuncDllInitialize)GetProcAddress(hModule, "DllInitialize");
    auto DllExecute = (FuncDllExecute)GetProcAddress(hModule, "DllExecute");
    auto DllWndProc = (FuncDllWndProc)GetProcAddress(hModule, "DllWndProc");

    if (!DllInitialize || !DllExecute || !DllWndProc) {
        spdlog::error("Missing required exports: {}", dllName);

        FreeLibrary(hModule);

        return 1;
    }

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DllWndProc; // Injecting the game module window procedure
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    // Assign window icons
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));

    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExA(&wc);

    spdlog::info("Creating window...");

    HWND hWnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        720,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    int exitCode = -1;

    if (hWnd) {
        ShowWindow(hWnd, nShowCmd);
        UpdateWindow(hWnd);

        // Initialize with the window handle
        spdlog::info("Calling DllInitialize()...");
        DllInitialize(&hWnd);

        // Block execution here
        // The game runs its own loop internally
        spdlog::info("Calling DllExecute(). Game is running...");
        exitCode = DllExecute();

        spdlog::info("Game closed (exit code: {})", exitCode);

        // Destroy the window safely before unloading the DLL
        if (IsWindow(hWnd)) {
            DestroyWindow(hWnd);

            spdlog::info("The window fell into the void!");
        }
    }
    else {
        spdlog::error("Failed to create window. Error: {}", GetLastError());
    }

    // Cleanup
    UnregisterClassA(WINDOW_CLASS_NAME, hInstance);
    FreeLibrary(hModule);

    // Game swap logic
    std::string nextOpt = "";

    if (opt == OPT_QUARTET && exitCode > 1) {
        switch (exitCode) {
        case 2: nextOpt = OPT_BR; break;
        case 3: nextOpt = OPT_RAY; break;
        case 4: nextOpt = OPT_SUN; break;
        case 5: nextOpt = OPT_TIE; break;
        default: nextOpt = ""; break;
        }
    }
    else if (opt != OPT_QUARTET && exitCode == 1) {
        // Return to the main launcher menu (B0.dll)
        nextOpt = OPT_QUARTET;
    }

    if (!nextOpt.empty()) {
        spdlog::info("Spawning fresh process for target: {}", nextOpt);

        std::string cmdLineStr = "\"" + exePath + "\" " + nextOpt;
        std::vector<char> cmdBuffer(cmdLineStr.begin(), cmdLineStr.end());
        cmdBuffer.push_back('\0');

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcessA(exePath.c_str(), cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, currentDir.c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            spdlog::info("Process spawned successfully");
        }
        else {
            spdlog::error("Failed to spawn process. Error: {}", GetLastError());
        }
    }

    spdlog::info("Exiting launcher...");

    // Dispose things here
    CoUninitialize();

    return 0;
}
