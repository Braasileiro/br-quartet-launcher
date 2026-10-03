#include <windows.h>
#include <charconv>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>
#include "logger.hpp"
#include "resources.h"

// Constants
constexpr auto LOG_FILENAME{"br-quartet-launcher.log"};

constexpr auto WINDOW_NAME{"BLUE REFLECTION Quartet"};
constexpr auto WINDOW_CLASS_NAME{"DX11LauncherWndClass"};

constexpr auto OPT_QUARTET{"quartet"};
constexpr auto OPT_BR{"br"};
constexpr auto OPT_RAY{"ray"};
constexpr auto OPT_SUN{"sun"};
constexpr auto OPT_TIE{"tie"};
constexpr std::string_view PREVIOUS_MODULE_ARG{"--previous-module="};

constexpr auto DLL_QUARTET{"B0.dll"};
constexpr auto DLL_BR{"B1.dll"};
constexpr auto DLL_RAY{"B2.dll"};
constexpr auto DLL_SUN{"B3.dll"};
constexpr auto DLL_TIE{"B4.dll"};

struct GameModule {
    int id;
    std::string_view option;
    const char *dllName;
};

constexpr GameModule GAME_MODULES[] = {
    {1, OPT_QUARTET, DLL_QUARTET},
    {2, OPT_BR, DLL_BR},
    {3, OPT_RAY, DLL_RAY},
    {4, OPT_SUN, DLL_SUN},
    {5, OPT_TIE, DLL_TIE}
};

// Signatures
struct DllInitializeArgs {
    HWND window;
    int previousModule;
};

static_assert(offsetof(DllInitializeArgs, previousModule) == 8);

typedef void (*FuncDllInitialize)(DllInitializeArgs *);
typedef int (*FuncDllExecute)();
typedef LRESULT (CALLBACK*FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

// Utils
static FuncDllWndProc DllWndProc = nullptr;

static const GameModule *FindModuleById(int id) {
    for (const auto &module : GAME_MODULES) {
        if (module.id == id) {
            return &module;
        }
    }

    return nullptr;
}

static const GameModule *FindModuleByOption(std::string_view option) {
    for (const auto &module : GAME_MODULES) {
        if (module.option == option) {
            return &module;
        }
    }

    return nullptr;
}

static LRESULT CALLBACK LauncherWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    if (DllWndProc) {
        return DllWndProc(hWnd, message, wParam, lParam);
    }

    return DefWindowProcA(hWnd, message, wParam, lParam);
}

static LONG WINAPI CrashHandler(const EXCEPTION_POINTERS *ExceptionInfo) {
    if (std::ofstream crashLog(LOG_FILENAME, std::ios::app); crashLog.is_open()) {
        crashLog << "fatal: Crashed! Exception code: 0x"
                << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode
                << std::dec << std::endl;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

// Entry
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nShowCmd) {
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

    const GameModule *currentModule = FindModuleByOption(OPT_QUARTET);
    int previousModule = -1;

    // Handle initial argument for direct game boot
    if (__argc > 1) {
        const GameModule *requestedModule = FindModuleByOption(__argv[1]);

        if (requestedModule) {
            currentModule = requestedModule;
        } else {
            spdlog::warn("Invalid parameter '{}'. Falling back to '{}'", __argv[1], OPT_QUARTET);
        }
    }

    // Child launcher processes use this argument to preserve the module transition state
    if (__argc > 2) {
        const std::string_view argument = __argv[2];

        if (argument.starts_with(PREVIOUS_MODULE_ARG)) {
            const std::string_view value = argument.substr(PREVIOUS_MODULE_ARG.size());
            int parsedModule = -1;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsedModule);

            if (error == std::errc() && end == value.data() + value.size() && FindModuleById(parsedModule)) {
                previousModule = parsedModule;
            } else {
                spdlog::warn("Invalid previous module argument: '{}'", argument);
            }
        }
    }

    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LauncherWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // Assign window icons
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));

    wc.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassA(&wc)) {
        spdlog::error("Failed to register window class. Error: {}", GetLastError());
        return 1;
    }

    spdlog::info("Creating window...");

    int windowX = CW_USEDEFAULT;
    int windowY = CW_USEDEFAULT;
    MONITORINFO monitorInfo = {.cbSize = sizeof(monitorInfo)};
    const HMONITOR monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);

    if (GetMonitorInfoA(monitor, &monitorInfo)) {
        // Match the original launcher's centered 1280x720 outer window
        windowX = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - 1280) / 2;
        windowY = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - 720) / 2;
    }

    HWND hWnd = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WINDOW_CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        1280,
        720,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd) {
        spdlog::error("Failed to create window. Error: {}", GetLastError());
        UnregisterClassA(WINDOW_CLASS_NAME, hInstance);
        return 1;
    }

    ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);

    spdlog::info("Current target: {}", currentModule->option);
    spdlog::info("Loading '{}' into memory...", currentModule->dllName);

    HMODULE hModule = LoadLibraryA(currentModule->dllName);
    int launcherExitCode = 0;
    int nextModuleId = -1;

    if (!hModule) {
        spdlog::error("Failed to load '{}'! Error: {}", currentModule->dllName, GetLastError());
        launcherExitCode = 1;
    } else {
        auto DllInitialize = reinterpret_cast<FuncDllInitialize>(GetProcAddress(hModule, "DllInitialize"));
        auto DllExecute = reinterpret_cast<FuncDllExecute>(GetProcAddress(hModule, "DllExecute"));
        DllWndProc = reinterpret_cast<FuncDllWndProc>(GetProcAddress(hModule, "DllWndProc"));

        if (!DllInitialize || !DllExecute || !DllWndProc) {
            spdlog::error("Missing required exports: {}", currentModule->dllName);
            launcherExitCode = 1;
        } else {
            DllInitializeArgs initializeArgs = {
                .window = hWnd,
                .previousModule = previousModule
            };

            spdlog::info("Calling DllInitialize() with previous module {}...", previousModule);
            DllInitialize(&initializeArgs);

            // The module runs its own message loop and returns the next module ID
            spdlog::info("Calling DllExecute(). Game is running...");
            nextModuleId = DllExecute();

            spdlog::info("Module closed (next module: {})", nextModuleId);
        }

        DllWndProc = nullptr;
        FreeLibrary(hModule);
    }

    if (IsWindow(hWnd)) {
        DestroyWindow(hWnd);
        spdlog::info("The window fell into the void!");
    }

    UnregisterClassA(WINDOW_CLASS_NAME, hInstance);

    if (launcherExitCode == 0 && nextModuleId != -1) {
        const GameModule *nextModule = FindModuleById(nextModuleId);

        if (!nextModule) {
            spdlog::error("Invalid module ID returned by '{}': {}", currentModule->dllName, nextModuleId);
            launcherExitCode = 1;
        } else {
            spdlog::info("Spawning isolated process for target: {}", nextModule->option);

            const std::string commandLine = "\"" + exePath + "\" "
                    + std::string(nextModule->option) + " "
                    + std::string(PREVIOUS_MODULE_ARG) + std::to_string(currentModule->id);
            std::vector commandBuffer(commandLine.begin(), commandLine.end());
            commandBuffer.push_back('\0');

            STARTUPINFOA startupInfo = {.cb = sizeof(startupInfo)};
            PROCESS_INFORMATION processInfo = {};

            if (CreateProcessA(
                    exePath.c_str(),
                    commandBuffer.data(),
                    nullptr,
                    nullptr,
                    FALSE,
                    0,
                    nullptr,
                    currentDir.c_str(),
                    &startupInfo,
                    &processInfo
            )) {
                CloseHandle(processInfo.hProcess);
                CloseHandle(processInfo.hThread);
                spdlog::info("Process spawned successfully");
            } else {
                spdlog::error("Failed to spawn process. Error: {}", GetLastError());
                launcherExitCode = 1;
            }
        }
    }

    spdlog::info("Exiting launcher...");

    return launcherExitCode;
}
