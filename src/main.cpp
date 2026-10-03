#include <windows.h>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>
#include "logger.hpp"
#include "resources.h"

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

// DLL interface
struct DllInitializeArgs {
    HWND window;
    int previousModule;
};

static_assert(offsetof(DllInitializeArgs, previousModule) == 8);

typedef void (*FuncDllInitialize)(DllInitializeArgs *);
typedef int (*FuncDllExecute)();
typedef LRESULT (CALLBACK*FuncDllWndProc)(HWND, UINT, WPARAM, LPARAM);

struct LaunchContext {
    const GameModule *module;
    int previousModule;
};

struct ModuleRunResult {
    bool succeeded;
    int nextModuleId;
};

static FuncDllWndProc g_dllWndProc = nullptr;

class UniqueModuleHandle {
public:
    explicit UniqueModuleHandle(HMODULE handle) : handle_(handle) {
    }

    ~UniqueModuleHandle() {
        if (handle_) {
            FreeLibrary(handle_);
        }
    }

    UniqueModuleHandle(const UniqueModuleHandle &) = delete;
    UniqueModuleHandle &operator=(const UniqueModuleHandle &) = delete;

    [[nodiscard]] HMODULE Get() const {
        return handle_;
    }

private:
    HMODULE handle_;
};

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE handle) : handle_(handle) {
    }

    ~UniqueHandle() {
        if (handle_) {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle &) = delete;
    UniqueHandle &operator=(const UniqueHandle &) = delete;

private:
    HANDLE handle_;
};

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

    if (g_dllWndProc) {
        return g_dllWndProc(hWnd, message, wParam, lParam);
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

static LaunchContext ParseCommandLine(int argc, char *argv[]) {
    LaunchContext context = {
        .module = FindModuleByOption(OPT_QUARTET),
        .previousModule = -1
    };

    if (argc > 1) {
        const GameModule *requestedModule = FindModuleByOption(argv[1]);

        if (requestedModule) {
            context.module = requestedModule;
        } else {
            spdlog::warn("Invalid parameter '{}'. Falling back to '{}'", argv[1], OPT_QUARTET);
        }
    }

    // Child launcher processes use this argument to preserve the module transition state
    if (argc > 2) {
        const std::string_view argument = argv[2];

        if (argument.starts_with(PREVIOUS_MODULE_ARG)) {
            const std::string_view value = argument.substr(PREVIOUS_MODULE_ARG.size());
            int parsedModule = -1;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsedModule);

            if (error == std::errc() && end == value.data() + value.size() && FindModuleById(parsedModule)) {
                context.previousModule = parsedModule;
            } else {
                spdlog::warn("Invalid previous module argument: '{}'", argument);
            }
        }
    }

    return context;
}

static HWND CreateLauncherWindow(HINSTANCE hInstance, int nShowCmd) {
    WNDCLASSA windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = LauncherWndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    windowClass.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassA(&windowClass)) {
        spdlog::error("Failed to register window class. Error: {}", GetLastError());
        return nullptr;
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

    HWND window = CreateWindowExA(
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

    if (!window) {
        spdlog::error("Failed to create window. Error: {}", GetLastError());
        UnregisterClassA(WINDOW_CLASS_NAME, hInstance);
        return nullptr;
    }

    ShowWindow(window, nShowCmd);
    UpdateWindow(window);

    return window;
}

static void DestroyLauncherWindow(HWND window, HINSTANCE hInstance) {
    if (IsWindow(window)) {
        DestroyWindow(window);
        spdlog::info("Window destroyed");
    }

    UnregisterClassA(WINDOW_CLASS_NAME, hInstance);
}

static ModuleRunResult RunModule(const GameModule &module, int previousModule, HWND window) {
    spdlog::info("Current target: {}", module.option);
    spdlog::info("Loading '{}' into memory...", module.dllName);

    UniqueModuleHandle moduleHandle(LoadLibraryA(module.dllName));

    if (!moduleHandle.Get()) {
        spdlog::error("Failed to load '{}'! Error: {}", module.dllName, GetLastError());
        return {false, -1};
    }

    auto DllInitialize = reinterpret_cast<FuncDllInitialize>(GetProcAddress(moduleHandle.Get(), "DllInitialize"));
    auto DllExecute = reinterpret_cast<FuncDllExecute>(GetProcAddress(moduleHandle.Get(), "DllExecute"));
    g_dllWndProc = reinterpret_cast<FuncDllWndProc>(GetProcAddress(moduleHandle.Get(), "DllWndProc"));

    if (!DllInitialize || !DllExecute || !g_dllWndProc) {
        spdlog::error("Missing required exports: {}", module.dllName);
        g_dllWndProc = nullptr;
        return {false, -1};
    }

    DllInitializeArgs initializeArgs = {
        .window = window,
        .previousModule = previousModule
    };

    spdlog::info("Calling DllInitialize() with previous module {}...", previousModule);
    DllInitialize(&initializeArgs);

    // The module runs its own message loop and returns the next module ID
    spdlog::info("Calling DllExecute(). Game is running...");
    const int nextModuleId = DllExecute();

    spdlog::info("Module closed (next module: {})", nextModuleId);

    g_dllWndProc = nullptr;

    return {true, nextModuleId};
}

static bool LaunchModuleProcess(
    const std::string &exePath,
    const std::string &currentDir,
    const GameModule &currentModule,
    const GameModule &nextModule
) {
    spdlog::info("Launching isolated process for target: {}", nextModule.option);

    const std::string commandLine = "\"" + exePath + "\" "
            + std::string(nextModule.option) + " "
            + std::string(PREVIOUS_MODULE_ARG) + std::to_string(currentModule.id);
    std::vector commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back('\0');

    STARTUPINFOA startupInfo = {.cb = sizeof(startupInfo)};
    PROCESS_INFORMATION processInfo = {};

    if (!CreateProcessA(
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
        spdlog::error("Failed to launch process. Error: {}", GetLastError());
        return false;
    }

    UniqueHandle processHandle(processInfo.hProcess);
    UniqueHandle threadHandle(processInfo.hThread);

    spdlog::info("Process launched successfully");

    return true;
}

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

    const LaunchContext context = ParseCommandLine(__argc, __argv);
    HWND window = CreateLauncherWindow(hInstance, nShowCmd);

    if (!window) {
        return 1;
    }

    const ModuleRunResult moduleResult = RunModule(*context.module, context.previousModule, window);
    DestroyLauncherWindow(window, hInstance);

    int launcherExitCode = moduleResult.succeeded ? 0 : 1;

    if (moduleResult.succeeded && moduleResult.nextModuleId != -1) {
        const GameModule *nextModule = FindModuleById(moduleResult.nextModuleId);

        if (!nextModule) {
            spdlog::error(
                "Invalid module ID returned by '{}': {}",
                context.module->dllName,
                moduleResult.nextModuleId
            );
            launcherExitCode = 1;
        } else if (!LaunchModuleProcess(exePath, currentDir, *context.module, *nextModule)) {
            launcherExitCode = 1;
        }
    }

    spdlog::info("Exiting launcher...");

    return launcherExitCode;
}
