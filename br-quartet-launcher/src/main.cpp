#include <windows.h>
#include <string>
#include <fstream>
#include <map>
#include <vector>

#include "resource.h"
#include "logger.hpp"
#include "version_info.h"

// Constants
constexpr LPCSTR LOG_FILENAME{ "br-quartet-launcher.log" };
constexpr LPCSTR WINDOW_NAME{ "BLUE REFLECTION Quartet" };
const std::string WINDOW_CLASS_NAME{ "BlueReflectionCustomWindow" };
constexpr LPCSTR CHOICE_QUARTET{ "quartet" };
constexpr LPCSTR CHOICE_BR{ "br" };
constexpr LPCSTR CHOICE_RAY{ "ray" };
constexpr LPCSTR CHOICE_SUN{ "sun" };
constexpr LPCSTR CHOICE_TIE{ "tie" };
constexpr LPCSTR DLL_QUARTET{ "B0.dll" };
constexpr LPCSTR DLL_BR{ "B1.dll" };
constexpr LPCSTR DLL_RAY{ "B2.dll" };
constexpr LPCSTR DLL_SUN{ "B3.dll" };
constexpr LPCSTR DLL_TIE{ "B4.dll" };

// Game function signatures
typedef void (*FuncDllInitialize)(HWND*);
typedef int (*FuncDllExecute)(void);
typedef LRESULT(CALLBACK* FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

// Global exception handler
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
    SetUnhandledExceptionFilter(CrashHandler);

    // Truncates the log on first launch only
    bool isFirstLaunch = (__argc <= 1);
    Logger::Init(LOG_FILENAME, isFirstLaunch);

    spdlog::info("==================================================");
    spdlog::info("br-quartet-launcher {} initialized", APP_PRODUCT_VERSION_A);
    spdlog::info("Command line: {}", GetCommandLineA());

    // Game mapping
    std::map<std::string, std::string> games = {
        {CHOICE_QUARTET, DLL_QUARTET},
        {CHOICE_BR,      DLL_BR},
        {CHOICE_RAY,     DLL_RAY},
        {CHOICE_SUN,     DLL_SUN},
        {CHOICE_TIE,     DLL_TIE}
    };

    std::string choice = CHOICE_QUARTET;

    // Handle initial argument for direct game boot
    if (__argc > 1) {
        choice = __argv[1];

        if (games.find(choice) == games.end()) {
            spdlog::warn("Invalid parameter '{}'. Falling back to '{}'", choice, CHOICE_QUARTET);

            choice = CHOICE_QUARTET;
        }
    }

    std::string dllName = games[choice];

    spdlog::info("Current target: {}", choice);
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

    // Dynamic class name mapped to the current choice
    std::string className = WINDOW_CLASS_NAME + "_" + choice;

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
    wc.lpszClassName = className.c_str();

    RegisterClassExA(&wc);

    spdlog::info("Creating window...");

    HWND hWnd = CreateWindowExA(
        0,
        className.c_str(),
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
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
    UnregisterClassA(className.c_str(), hInstance);
    FreeLibrary(hModule);

    // Game swap logic
    std::string nextChoice = "";

    if (choice == CHOICE_QUARTET && exitCode > 1) {
        switch (exitCode) {
        case 2: nextChoice = CHOICE_BR; break;
        case 3: nextChoice = CHOICE_RAY; break;
        case 4: nextChoice = CHOICE_SUN; break;
        case 5: nextChoice = CHOICE_TIE; break;
        default: nextChoice = ""; break;
        }
    }
    else if (choice != CHOICE_QUARTET && exitCode == 1) {
        // Return to the main launcher menu (B0.dll)
        nextChoice = CHOICE_QUARTET;
    }

    if (!nextChoice.empty()) {
        spdlog::info("Spawning fresh process for target: {}", nextChoice);

        char szPath[MAX_PATH];
        GetModuleFileNameA(NULL, szPath, MAX_PATH);

        std::string cmdLineStr = "\"" + std::string(szPath) + "\" " + nextChoice;

        std::vector<char> cmdBuffer(cmdLineStr.begin(), cmdLineStr.end());
        cmdBuffer.push_back('\0');

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcessA(szPath, cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            spdlog::info("Process spawned successfully");
        }
        else {
            spdlog::error("Failed to spawn process. Error: {}", GetLastError());
        }
    }

    spdlog::info("Exiting launcher...");

    return 0;
}
