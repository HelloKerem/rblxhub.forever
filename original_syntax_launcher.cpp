#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <TlHelp32.h>
#include <Psapi.h>
#include <comdef.h>
#include <sstream>
#include <shlobj.h>
#include <thread>


// no i didnt make this function, this looks like absolute hell
int ReplaceAllStringsInMemory(DWORD pid, const char* search, const char* replace, bool forceReplace = false) {
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!hProcess) {
        std::cerr << "Failed to open process.\n";
        return 0;
    }

    SIZE_T bytesRead, bytesWritten;
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    MEMORY_BASIC_INFORMATION mbi;
    BYTE* addr = (BYTE*)sysInfo.lpMinimumApplicationAddress;
    SIZE_T searchLen = strlen(search);
    SIZE_T replaceLen = strlen(replace);
    int count = 0;

    while (addr < sysInfo.lpMaximumApplicationAddress) {
        if (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) == sizeof(mbi) && mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS) {
            std::vector<BYTE> buffer(mbi.RegionSize);
            if (ReadProcessMemory(hProcess, addr, buffer.data(), mbi.RegionSize, &bytesRead)) {
                for (SIZE_T i = 0; i < bytesRead - searchLen; i++) {
                    if (memcmp(&buffer[i], search, searchLen) == 0) {
                        // keep debug print for now
                       // std::cout << "Found occurrence at: " << std::hex << (uintptr_t)(addr + i) << "\n";
                        if (forceReplace)
                        {
                            WriteProcessMemory(hProcess, addr + i, replace, replaceLen, &bytesWritten);
                        }

                        if (replaceLen < searchLen) {
                            SIZE_T paddingSize = searchLen - replaceLen;
                            std::vector<BYTE> nullBytes(paddingSize, 0);
                            if (forceReplace)
                            {
                                WriteProcessMemory(hProcess, addr + i + replaceLen, nullBytes.data(), paddingSize, &bytesWritten);
                            }
                        }

                        count++;
                    }
                }
            }
        }
        addr += mbi.RegionSize;
    }

    CloseHandle(hProcess);
    return count;
}

DWORD LaunchRobloxProcess(int argc, char* argv[]) {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    const char* exePath = "RobloxPlayerBeta.exe";

    std::string commandLine = "\"" + std::string(exePath) + "\"";

    for (int i = 1; i < argc; i++) {
        commandLine += " ";

        std::string arg = argv[i];
        if (arg.find(' ') != std::string::npos) {
            commandLine += "\"" + arg + "\"";
        }
        else {
            commandLine += arg;
        }
    }

    if (argc == 1) {
        commandLine += " --app";
    }

    if (commandLine.length() > 8191) {
        std::cerr << "Command line too long\n";
        return 0;
    }

    if (!CreateProcessA(NULL, const_cast<char*>(commandLine.c_str()),
        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to start RobloxPlayerBeta.exe\n";
        return 0;
    }

    CloseHandle(pi.hThread);
    return pi.dwProcessId;
}

// no i did not make the below 2 functions this is all windows api stuff
bool SuspendProcess(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread) {
                    SuspendThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te32));
    }

    CloseHandle(hSnapshot);
    return true;
}

bool ResumeProcess(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(hSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
                if (hThread) {
                    ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnapshot, &te32));
    }

    CloseHandle(hSnapshot);
    return true;
}
// ktrain code
void ReplaceUntilThreshold(DWORD pid, const char* search, const char* replace, int threshold = 9) {
    // this is so badly hacked together
    while (true) {
        if (!SuspendProcess(pid)) { // do we really need to have this in a check
            return;
        }
        ResumeProcess(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        if (!SuspendProcess(pid)) {
            return;
        }

        int replacedCount = ReplaceAllStringsInMemory(pid, search, replace);
       // std::cout << "Replaced occurrences: " << replacedCount << "\n"; // only print we should keep for debug

        // this looks very confusing, but what we do is that we basically see how many occurences there are
        // of something like "roblox.qq.com" - for that string, the correct amount would be 12
        // so basically we keep suspending, checking, and resuming until we get that right number amount
        if (replacedCount >= threshold) {
            ReplaceAllStringsInMemory(pid, search, replace, true);
            // todo: try to integrate mutex into the function itself (should speed up startup times)
            // the "true" is forceReplace, which esentially just actually makes it so the func replaces the strings (before that we're just checking occurences)
            ReplaceAllStringsInMemory(pid, "ROBLOX_singletonMutex", "ROBLOS_singletonMutex", true);
            ReplaceAllStringsInMemory(pid, "ROBLOX_singletonEvent", "ROBLOS_singletonEvent", true);
           // std::cout << "Threshold reached. Stopping replacement.\n";
            std::cout << "Client success.\n";
            break;
        }
    }
    ResumeProcess(pid);
}

int main(int argc, char* argv[]) {
    DWORD pid = LaunchRobloxProcess(argc, argv);
    if (pid != 0) {
       // printf("Got roblox: pid = %d\n", pid);
        printf("Please wait while we try to open 2025 client!");
        ReplaceUntilThreshold(pid, "roblox.qq.com", "synt2x.xyz");
    }
    return 0;
}