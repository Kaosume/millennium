﻿/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define UNICODE
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#define _WINSOCKAPI_
#endif
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <log.h>
#include "loader.h"
#include "co_spawn.h"
#include <serv.h>
#include <signal.h>
#include <cxxabi.h>
#include "terminal_pipe.h"
#include "executor.h"
#include <env.h>

/**
 * @brief Verify the environment to ensure that the CEF remote debugging is enabled.
 * .cef-enable-remote-debugging is a special file name that Steam uses to signal CEF to enable remote debugging.
 */
const static void VerifyEnvironment() 
{
    const auto filePath = SystemIO::GetSteamPath() / ".cef-enable-remote-debugging";

    // Steam's CEF Remote Debugger isn't exposed to port 8080
    if (!std::filesystem::exists(filePath)) 
    {
        std::ofstream(filePath).close();

        Logger.Log("Successfully enabled CEF remote debugging, you can now restart Steam...");
        std::exit(1);
    }
}

/**
 * @brief Custom terminate handler for Millennium.
 * This function is called when Millennium encounters a fatal error that it can't recover from.
 */
void OnTerminate() 
{
    #ifdef _WIN32
    if (IsDebuggerPresent()) __debugbreak();
    #endif

    auto const exceptionPtr = std::current_exception();
    std::string errorMessage = "Millennium has a fatal error that it can't recover from, check the logs for more details!\n";

    if (exceptionPtr) 
    {
        try 
        {
            int status;
            errorMessage.append(fmt::format("Terminating with uncaught exception of type `{}`", abi::__cxa_demangle(abi::__cxa_current_exception_type()->name(), 0, 0, &status)));
            std::rethrow_exception(exceptionPtr); // rethrow the exception to catch its exception message
        }
        catch (const std::exception& e) 
        {
            errorMessage.append(fmt::format("with `what()` = \"{}\"", e.what()));
        }
        catch (...) { }
    }

    #ifdef _WIN32
    MessageBoxA(NULL, errorMessage.c_str(), "Oops!", MB_ICONERROR | MB_OK);
    #elif __linux__
    std::cerr << errorMessage << std::endl;
    #endif
}

#ifdef __linux__
#include <sys/ptrace.h>
#include <unistd.h>

/**
 * @brief Check if a debugger is present on Linux.
 * @return True if a debugger is present, false otherwise.
 */
int IsDebuggerPresent() 
{
    return ptrace(PTRACE_TRACEME, 0, 0, 0) == -1;
}

#endif

/**
 * @brief Millennium's main method, called on startup on both Windows and Linux.
 */
const static void EntryMain() 
{
    #if _WIN32
    SetupEnvironmentVariables();
    #endif

    if (!IsDebuggerPresent()) 
    {
        std::set_terminate(OnTerminate); // Set custom terminate handler for easier debugging
    }
    
    /** Handle signal interrupts (^C) */
    signal(SIGINT, [](int signalCode) { std::exit(128 + SIGINT); });

    #ifdef _WIN32
    /**
    * Windows requires a special environment setup to redirect stdout to a pipe.
    * This is necessary for the logger component to capture stdout from Millennium.
    * This is also necessary to update the updater module from cache.
    */
    WinUtils::SetupWin32Environment();  
    #endif 

    /**
     * Create an FTP server to allow plugins to be loaded from the host machine.
     */
    uint16_t ftpPort = Crow::CreateAsyncServer();

    const auto startTime = std::chrono::system_clock::now();
    VerifyEnvironment();

    std::shared_ptr<PluginLoader> loader = std::make_shared<PluginLoader>(startTime, ftpPort);
    SetPluginLoader(loader);

    PythonManager& manager = PythonManager::GetInstance();

    /** Start the python backends */
    auto backendThread   = std::thread([&loader, &manager] { loader->StartBackEnds(manager); });
    /** Start the injection process into the Steam web helper */
    auto frontendThreads = std::thread([&loader] { loader->StartFrontEnds(); });

    backendThread  .join();
    frontendThreads.detach();
}

__attribute__((constructor)) void __init_millennium() 
{
    std::cout << "Millennium loaded" << std::endl;
    SetupEnvironmentVariables();
}

#ifdef _WIN32
HANDLE g_hMillenniumThread;
/**
 * @brief Entry point for Millennium on Windows.
 * @param fdwReason The reason for calling the DLL.
 * @return True if the DLL was successfully loaded, false otherwise.
 */
int __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) 
    {
        case DLL_PROCESS_ATTACH: 
        {
            const std::string threadName = fmt::format("Millennium@{}", MILLENNIUM_VERSION);

            g_hMillenniumThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)EntryMain, NULL, 0, NULL);
            SetThreadDescription(g_hMillenniumThread, std::wstring(threadName.begin(), threadName.end()).c_str());
            break;
        }
        case DLL_PROCESS_DETACH: 
        {
            WinUtils::RestoreStdout();
            Logger.PrintMessage(" MAIN ", "Shutting Millennium down...", COL_MAGENTA);

            std::exit(0);
            break;
        }
    }

    return true;
}

#elif __linux__
#include <stdio.h>
#include <stdlib.h>
#include "helpers.h"
#include <dlfcn.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <stdexcept>

extern "C"
{
    /* Trampoline for the real main() */
    static int (*fnMainOriginal)(int, char **, char **);
    std::unique_ptr<std::thread> g_millenniumThread;

    /** 
     * Since this isn't an executable, and is "preloaded", the kernel doesn't implicitly load dependencies, so we need to manually. 
    */
    static constexpr const char* __LIBPYTHON_RUNTIME_PATH = LIBPYTHON_RUNTIME_PATH;

    /* Our fake main() that gets called by __libc_start_main() */
    int MainHooked(int argc, char **argv, char **envp)
    {
        Logger.Log("Hooked main() with PID: {}", getpid());
        Logger.Log("Loading python libraries from {}", __LIBPYTHON_RUNTIME_PATH);

        if (!dlopen(__LIBPYTHON_RUNTIME_PATH, RTLD_LAZY | RTLD_GLOBAL)) 
        {
            LOG_ERROR("Failed to load python libraries: {},\n\nThis is likely because it was not found on disk, try reinstalling Millennium.", dlerror());
        }

        #ifdef MILLENNIUM_SHARED
        {
            /** Start Millennium on a new thread to prevent I/O blocking */
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            int steam_main = fnMainOriginal(argc, argv, envp);
            Logger.Log("Hooked Steam entry returned {}", steam_main);

            g_threadTerminateFlag->flag.store(true);
            Sockets::Shutdown();
            g_millenniumThread->join();

            Logger.Log("Shutting down Millennium...");
            return steam_main;
        }
        #else
        {
            g_threadTerminateFlag->flag.store(true);
            g_millenniumThread = std::make_unique<std::thread>(EntryMain);
            g_millenniumThread->join();
            return 0;
        }
        #endif
    }

    void RemoveFromLdPreload() 
    {
        const char* ldPreload = getenv("LD_PRELOAD");
        if (!ldPreload)  
        {
            fprintf(stderr, "LD_PRELOAD is not set.\n");
            return;
        }

        char* ldPreloadStr = strdup(ldPreload);
        if (!ldPreloadStr) 
        {
            perror("strdup");
            return;
        }

        char* token, *rest = ldPreloadStr;
        size_t tokenCount = 0, tokenArraySize = 8; 
        char** tokens = (char**)malloc(tokenArraySize * sizeof(char*));

        if (!tokens) 
        {
            perror("malloc");
            free(ldPreloadStr);
            return;
        }

        while ((token = strtok_r(rest, " ", &rest))) 
        {
            if (strcmp(token, GetEnv("MILLENNIUM_RUNTIME_PATH").c_str()) != 0) 
            {
                if (tokenCount >= tokenArraySize) 
                {
                    tokenArraySize *= 2;
                    char** temp = (char**)realloc(tokens, tokenArraySize * sizeof(char*));
                    if (!temp) 
                    {
                        perror("realloc");
                        free(ldPreloadStr);
                        free(tokens);
                        return;
                    }
                    tokens = temp;
                }
                tokens[tokenCount++] = token;
            }
        }

        size_t newSize = 0;
        for (size_t i = 0; i < tokenCount; ++i) 
        {
            newSize += strlen(tokens[i]) + 1;
        }

        char* updatedLdPreload = (char*)malloc(newSize > 0 ? newSize : 1);
        if (!updatedLdPreload) 
        {
            perror("malloc");
            free(ldPreloadStr);
            free(tokens);
            return;
        }

        updatedLdPreload[0] = '\0';
        for (size_t i = 0; i < tokenCount; ++i) 
        {
            if (i > 0) 
            {
                strcat(updatedLdPreload, " ");
            }
            strcat(updatedLdPreload, tokens[i]);
        }

        printf("Updating LD_PRELOAD from [%s] to [%s]\n", ldPreloadStr, updatedLdPreload);

        if (setenv("LD_PRELOAD", updatedLdPreload, 1) != 0) 
        {
            perror("setenv");
        }

        free(ldPreloadStr);
        free(updatedLdPreload);
        free(tokens);
    }
    #ifdef MILLENNIUM_SHARED

    int IsSamePath(const char *path1, const char *path2) 
    {
        char realpath1[PATH_MAX], realpath2[PATH_MAX];
        struct stat stat1, stat2;

        // Get the real paths for both paths (resolves symlinks)
        if (realpath(path1, realpath1) == NULL) 
        {
            perror("realpath failed for path1");
            return 0;  // Error in resolving path
        }
        if (realpath(path2, realpath2) == NULL) 
        {
            perror("realpath failed for path2");
            return 0;  // Error in resolving path
        }

        // Compare resolved paths
        if (strcmp(realpath1, realpath2) != 0) 
        {
            return 0;  // Paths are different
        }

        // Check if both paths are symlinks and compare symlink targets
        if (lstat(path1, &stat1) == 0 && lstat(path2, &stat2) == 0) 
        {
            if (S_ISLNK(stat1.st_mode) && S_ISLNK(stat2.st_mode)) 
            {
                // Both are symlinks, compare the target paths
                char target1[PATH_MAX], target2[PATH_MAX];
                ssize_t len1 = readlink(path1, target1, sizeof(target1) - 1);
                ssize_t len2 = readlink(path2, target2, sizeof(target2) - 1);

                if (len1 == -1 || len2 == -1) 
                {
                    perror("readlink failed");
                    return 0;
                }

                target1[len1] = '\0';
                target2[len2] = '\0';

                // Compare the symlink targets
                if (strcmp(target1, target2) != 0) 
                {
                    return 0;  // Symlinks point to different targets
                }
            }
        }

        return 1;  // Paths are the same, including symlinks to each other
    }

    /*
    * Trampoline for __libc_start_main() that replaces the real main
    * function with our hooked version.
    */
    int __libc_start_main(
        int (*main)(int, char **, char **), int argc, char **argv,
        int (*init)(int, char **, char **), void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
    {
        /* Save the real main function address */
        fnMainOriginal = main;

        /* Get the address of the real __libc_start_main() */
        decltype(&__libc_start_main) orig = (decltype(&__libc_start_main))dlsym(RTLD_NEXT, "__libc_start_main");

        /** not loaded in a invalid child process */
        if (!IsSamePath(argv[0], GetEnv("MILLENNIUM__STEAM_EXE_PATH").c_str()))
        {
            return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
        }

        Logger.Log("Hooked __libc_start_main() {}", argv[0]);

        /* Remove the Millennium library from LD_PRELOAD */
        RemoveFromLdPreload();
        /* Log that we've loaded Millennium */
        Logger.Log("Loaded Millennium on {}, system architecture {}", GetLinuxDistro(), GetSystemArchitecture());
        /* ... and call it with our custom main function */
        return orig(MainHooked, argc, argv, init, fini, rtld_fini, stack_end);
    }
    #endif
}

int main(int argc, char **argv, char **envp)
{
    return MainHooked(argc, argv, envp);
}
#endif
