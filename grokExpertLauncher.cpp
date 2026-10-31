// This code is ai generated, the name of this file says it
// Build command: cl /O1 /Os /EHsc /MD grokExpertLauncher.cpp /link /OPT:REF /OPT:ICF

#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <TlHelp32.h>
#include <thread>
#include <chrono>

class MemoryPatcher {
public:
	struct ScanResult {
		int count = 0;
		std::vector<DWORD> protects;   // protect values of regions that contained the string
	};

	static ScanResult CountOrReplace(DWORD pid, const char* search, const char* replace, bool doReplace = false) {
		ScanResult result;
		if (!search || !replace) return result;

		HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (!hProcess) return result;

		SYSTEM_INFO si{};
		GetSystemInfo(&si);
		BYTE* addr = static_cast<BYTE*>(si.lpMinimumApplicationAddress);
		const SIZE_T searchLen = strlen(search);
		const SIZE_T replaceLen = strlen(replace);

		while (addr < si.lpMaximumApplicationAddress) {
			MEMORY_BASIC_INFORMATION mbi{};
			if (VirtualQueryEx(hProcess, addr, &mbi, sizeof(mbi)) != sizeof(mbi))
				break;

			if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_NOACCESS) || mbi.RegionSize == 0) {
				addr = static_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
				continue;
			}

			std::vector<BYTE> buffer(mbi.RegionSize);
			SIZE_T bytesRead = 0;
			if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead) && bytesRead > 0) {
				bool hasMatch = false;
				for (SIZE_T i = 0; i + searchLen <= bytesRead; ++i) {
					if (memcmp(buffer.data() + i, search, searchLen) == 0) {
						hasMatch = true;
						break;
					}
				}

				if (hasMatch) {
					result.protects.push_back(mbi.Protect);
				}

				for (SIZE_T i = 0; i + searchLen <= bytesRead; ++i) {
					if (memcmp(buffer.data() + i, search, searchLen) == 0) {
						if (doReplace) {
							SIZE_T written = 0;
							WriteProcessMemory(hProcess, static_cast<BYTE*>(mbi.BaseAddress) + i, replace, replaceLen, &written);
							if (replaceLen < searchLen) {
								std::vector<BYTE> zeros(searchLen - replaceLen, 0);
								WriteProcessMemory(hProcess, static_cast<BYTE*>(mbi.BaseAddress) + i + replaceLen, zeros.data(), zeros.size(), &written);
							}
						}
						result.count++;
					}
				}
			}
			addr = static_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
		}
		CloseHandle(hProcess);
		return result;
	}

    static bool SuspendProcess(DWORD pid) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        THREADENTRY32 te{};
        te.dwSize = sizeof(te);
        bool success = true;

        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (th) {
                        if (SuspendThread(th) == (DWORD)-1)
                            success = false;
                        CloseHandle(th);
                    }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        return success;
    }

    static bool ResumeProcess(DWORD pid) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        THREADENTRY32 te{};
        te.dwSize = sizeof(te);

        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == pid) {
                    HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (th) {
                        ResumeThread(th);
                        CloseHandle(th);
                    }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
        return true;
    }

    static bool IsProcessAlive(DWORD pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return false;
        DWORD exitCode = 0;
        GetExitCodeProcess(h, &exitCode);
        CloseHandle(h);
        return exitCode == STILL_ACTIVE;
    }
};

DWORD LaunchTarget(int argc, char** argv) {
    std::string cmd = "\"RobloxPlayerBeta.exe\"";

    for (int i = 1; i < argc; ++i) {
        cmd += " ";
        std::string arg = argv[i];
        if (arg.find(' ') != std::string::npos)
            cmd += "\"" + arg + "\"";
        else
            cmd += arg;
    }

    if (argc <= 1) // == 1)
        cmd += R"( -play -a "roblox://experiences/start?placeId=1")"; // " --app";

    if (cmd.size() > 8191)
        return 0;

    // CreateProcessA stringi değiştirebildiği için mutable kopya gerekli
    std::vector<char> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back('\0');

    STARTUPINFOA si{ sizeof(si) };
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi))
        return 0;

    CloseHandle(pi.hThread);
    return pi.dwProcessId;
}

void PatchUntilReady(DWORD pid, const char* oldDomain, const char* newDomain, int threshold = 9) {
    const int maxAttempts = 150;
    int attempts = 0;

    while (attempts++ < maxAttempts) {
        if (!MemoryPatcher::IsProcessAlive(pid)) {
            std::cerr << "Process unexpectedly closed.\n";
            return;
        }

        MemoryPatcher::SuspendProcess(pid);
        MemoryPatcher::ResumeProcess(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        MemoryPatcher::SuspendProcess(pid);

        auto res = MemoryPatcher::CountOrReplace(pid, oldDomain, newDomain, false);

        int writableCount = 0;
        for (DWORD p : res.protects) {
            if (p == PAGE_READWRITE || p == PAGE_EXECUTE_READWRITE)
                writableCount++;
        }

        if (res.count >= threshold) {	
            if (writableCount == 0) {
                std::cout << "No writable regions found, retrying...\n";
                MemoryPatcher::ResumeProcess(pid);
                continue;
            }

			std::cout << "Found " << res.count << " instance(s).\n";
            std::cout << "Threshold reached and writable regions found, writing memory.\n";
            auto writeRes = MemoryPatcher::CountOrReplace(pid, oldDomain, newDomain, true);
            std::cout << "Wrote " << writeRes.count << " instance(s).\n";
            std::cout << "Client success.\n";
            break;
        }
    }
    MemoryPatcher::ResumeProcess(pid);
}

int main(int argc, char** argv) {
    DWORD pid = LaunchTarget(argc, argv);
    if (!pid) {
        std::cerr << "Could not start RobloxPlayerBeta.exe.";
        return 1;
    }

    std::cout << "Process launched suspended, reading memory..." << "\n";
    PatchUntilReady(pid, "roblox.com", "rbolock.tk");
    return 0;
}