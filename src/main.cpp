#include <windows.h>
#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <objbase.h>
#include "logger.hpp"
#include "resources.h"

// General
constexpr auto LOG_FILENAME{ "br-quartet-launcher.log" };

// Window
constexpr auto WINDOW_NAME{ "BLUE REFLECTION Quartet" };
constexpr auto WINDOW_CLASS_NAME{ "DX11LauncherWndClass" };

// Options
constexpr auto OPT_QUARTET{ "quartet" };
constexpr auto OPT_BR{ "br" };
constexpr auto OPT_RAY{ "ray" };
constexpr auto OPT_SUN{ "sun" };
constexpr auto OPT_TIE{ "tie" };

// DLLs
constexpr auto DLL_QUARTET{ "B0.dll" };
constexpr auto DLL_BR{ "B1.dll" };
constexpr auto DLL_RAY{ "B2.dll" };
constexpr auto DLL_SUN{ "B3.dll" };
constexpr auto DLL_TIE{ "B4.dll" };

// Signatures
typedef void (*FuncDllInitialize)(HWND*);
typedef int (*FuncDllExecute)();
typedef LRESULT(CALLBACK* FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

// Global exception handler
static LONG WINAPI CrashHandler(const EXCEPTION_POINTERS* ExceptionInfo) {
    if (std::ofstream crashLog(LOG_FILENAME, std::ios::app); crashLog.is_open()) {
        crashLog << "fatal: Crashed! Exception code: 0x"
            << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode
            << std::dec << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nShowCmd)
{
    // Setting the current working directory
    char exePathRaw[MAX_PATH];
    GetModuleFileNameA(nullptr, exePathRaw, MAX_PATH);
    std::string exePath(exePathRaw);
    std::string::size_type pos = exePath.find_last_of("\\/");
    std::string currentDir = exePath.substr(0, pos);
    SetCurrentDirectoryA(currentDir.c_str());

    // Initialize global exception handler
    SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler));

    // Initialize global logger
    Logger::Init(LOG_FILENAME, true);
    spdlog::info("br-quartet-launcher {} initialized", APP_VERSION_STR);
    spdlog::info("Command line: {}", GetCommandLineA());

    // Initialize COM
    if (HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED); hrCom < 0) {
        spdlog::warn("CoInitializeEx() failed - HRESULT: 0x{:X}", static_cast<unsigned long>(hrCom));
    } else {
        spdlog::info("CoInitializeEx() success - HRESULT: 0x{:X}", static_cast<unsigned long>(hrCom));
    }

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

        if (!games.contains(opt)) {
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

    auto DllInitialize = reinterpret_cast<FuncDllInitialize>(GetProcAddress(hModule, "DllInitialize"));
    auto DllExecute = reinterpret_cast<FuncDllExecute>(GetProcAddress(hModule, "DllExecute"));
    auto DllWndProc = reinterpret_cast<FuncDllWndProc>(GetProcAddress(hModule, "DllWndProc"));

    if (!DllInitialize || !DllExecute || !DllWndProc) {
        spdlog::error("Missing required exports: {}", dllName);

        FreeLibrary(hModule);

        return 1;
    }

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DllWndProc; // Injecting the game module window procedure
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // Assign window icons
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));

    wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
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
        nullptr,
        nullptr,
        hInstance,
        nullptr
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
    std::string nextOpt;

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
        std::vector cmdBuffer(cmdLineStr.begin(), cmdLineStr.end());
        cmdBuffer.push_back('\0');

        STARTUPINFOA si = { .cb = sizeof(si) };
        PROCESS_INFORMATION pi;

        auto process = CreateProcessA(
            exePath.c_str(),
            cmdBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            currentDir.c_str(),
            &si,
            &pi
        );

        if (process) {
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
