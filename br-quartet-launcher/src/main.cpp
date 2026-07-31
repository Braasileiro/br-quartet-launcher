#include <windows.h>
#include <string>
#include <map>
#include <fstream>
#include "logger.hpp"
#include "version_info.h"

// Constants
const LPCSTR LOG_FILENAME{ "br-quartet-launcher.log" };
const LPCSTR WINDOW_NAME{ "BLUE REFLECTION Quartet" };
const std::string WINDOW_CLASS_NAME{ "BlueReflectionCustomWindow" };
const LPCSTR CHOICE_QUARTET{ "quartet" };
const LPCSTR CHOICE_BR{ "br" };
const LPCSTR CHOICE_RAY{ "ray" };
const LPCSTR CHOICE_SUN{ "sun" };
const LPCSTR CHOICE_TIE{ "tie" };
const LPCSTR DLL_QUARTET{ "B0.dll" };
const LPCSTR DLL_BR{ "B1.dll" };
const LPCSTR DLL_RAY{ "B2.dll" };
const LPCSTR DLL_SUN{ "B3.dll" };
const LPCSTR DLL_TIE{ "B4.dll" };

// Game function signatures
typedef void (*FuncDllInitialize)(HINSTANCE, HINSTANCE, LPSTR, int);
typedef int (*FuncDllExecute)(HINSTANCE, HINSTANCE, LPSTR, int);
typedef LRESULT(CALLBACK* FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

// WinAPI exception handler
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    std::ofstream crashLog(LOG_FILENAME, std::ios::app);

    if (crashLog.is_open()) {
        crashLog << "fatal: Crashed! Exception code: 0x"
            << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode
            << std::dec << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    SetUnhandledExceptionFilter(CrashHandler);

    // Truncates the log on first launch only
    bool isFirstLaunch = (__argc <= 1);
    Logger::Init(LOG_FILENAME, isFirstLaunch);

    LPSTR cmdLine = GetCommandLineA();

    spdlog::info("==================================================");
    spdlog::info("br-quartet-launcher {} initialized", APP_PRODUCT_VERSION_A);
    spdlog::info("Command line: {}", cmdLine);

    // Arg
    std::map<std::string, std::string> games = {
        {CHOICE_QUARTET, DLL_QUARTET},
        {CHOICE_BR,      DLL_BR},
        {CHOICE_RAY,     DLL_RAY},
        {CHOICE_SUN,     DLL_SUN},
        {CHOICE_TIE,     DLL_TIE}
    };

    std::string choice = CHOICE_QUARTET;

    if (__argc > 1) {
        choice = __argv[1];

        if (games.find(choice) == games.end()) {
            spdlog::warn("Invalid parameter '{}'. Falling back to '{}'", choice, CHOICE_QUARTET);

            choice = CHOICE_QUARTET;
        }
    }

    std::string dllName = games[choice];

    spdlog::info("Current target: {}", choice);
    spdlog::info("Loading '{}'...", dllName);

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

    spdlog::info("Calling DllInitialize()...");

    DllInitialize(hInstance, NULL, cmdLine, nShowCmd);

    // Dynamic class name
    std::string className = WINDOW_CLASS_NAME + "_" + choice;

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DllWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
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

    if (hWnd) {
        ShowWindow(hWnd, nShowCmd);
        UpdateWindow(hWnd);
    }

    spdlog::info("Calling DllExecute(). Game is running...");

    // Blocks execution!
    int exitCode = DllExecute(hInstance, NULL, cmdLine, nShowCmd);

    spdlog::info("Game closed (exit code: {})", exitCode);

    // Destroy and unregister current window safely
    if (hWnd != NULL && IsWindow(hWnd)) {
        DestroyWindow(hWnd);

        spdlog::info("The window fell into the void!");
    }

    UnregisterClassA(className.c_str(), hInstance);
    FreeLibrary(hModule);

    // Next action selector
    std::string nextArg = "";

    if (choice == CHOICE_QUARTET && exitCode > 1) {
        switch (exitCode) {
        case 2:
            nextArg = CHOICE_BR;
            break;
        case 3:
            nextArg = CHOICE_RAY;
            break;
        case 4:
            nextArg = CHOICE_SUN;
            break;
        case 5:
            nextArg = CHOICE_TIE;
            break;
        default:
            nextArg = "";
            break;
        }
    }
    else if (choice != CHOICE_QUARTET && exitCode == 1) {
        // RETURN TO TOP (B0.dll)
        nextArg = CHOICE_QUARTET;
    }

    if (!nextArg.empty()) {
        // Spawn a fresh process for the next state
        spdlog::info("Spawning process for target: {}", nextArg);

        char szPath[MAX_PATH];
        GetModuleFileNameA(NULL, szPath, MAX_PATH);

        std::string cmdLine = std::string("\"") + szPath + "\" " + nextArg;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        if (CreateProcessA(szPath, (LPSTR)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            spdlog::info("Process spawned successfully");
        }
        else {
            spdlog::error("Failed to spawn process. Error: {}", GetLastError());
        }
    }

    spdlog::info("Exiting...");

    return 0;
}
