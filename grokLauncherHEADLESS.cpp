// SubSystem = Windows (/SUBSYSTEM:WINDOWS)
// This code is ai generated, the name of this file says it
// Build command: cl /O1 /Os /EHsc /MD grokLauncherHEADLESS.cpp /link /OPT:REF /OPT:ICF

#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

class ProcessMemory {
public:
    explicit ProcessMemory(DWORD pid) {
        handle_ = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            FALSE, pid);
    }

    ~ProcessMemory() {
        if (handle_) CloseHandle(handle_);
    }

    bool IsValid() const { return handle_ != nullptr; }

    int ReplaceString(const std::string& search, const std::string& replace, bool doWrite) {
        if (!handle_ || search.empty()) return 0;

        SYSTEM_INFO si{};
        GetSystemInfo(&si);

        MEMORY_BASIC_INFORMATION mbi{};
        auto current = reinterpret_cast<BYTE*>(si.lpMinimumApplicationAddress);
        const auto maxAddr = reinterpret_cast<BYTE*>(si.lpMaximumApplicationAddress);

        int count = 0;
        const size_t searchLen = search.size();
        const size_t replaceLen = replace.size();

        while (current < maxAddr) {
            if (VirtualQueryEx(handle_, current, &mbi, sizeof(mbi)) != sizeof(mbi))
                break;

            bool readable = (mbi.State == MEM_COMMIT) &&
                            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));

            if (readable && mbi.RegionSize > 0) {
                std::vector<BYTE> buffer;
                try {
                    buffer.resize(mbi.RegionSize);
                } catch (...) {
                    current = static_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
                    continue;
                }

                SIZE_T bytesRead = 0;
                if (ReadProcessMemory(handle_, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead) && bytesRead >= searchLen) {
                    for (size_t i = 0; i + searchLen <= bytesRead; ++i) {
                        if (memcmp(buffer.data() + i, search.data(), searchLen) == 0) {
                            if (doWrite) {
                                DWORD oldProtect = 0;
                                bool changedProtect = false;

                                if (!(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
                                    if (VirtualProtectEx(handle_, mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProtect))
                                        changedProtect = true;
                                    else
                                        continue;
                                }

                                SIZE_T written = 0;
                                WriteProcessMemory(handle_,
                                    static_cast<BYTE*>(mbi.BaseAddress) + i,
                                    replace.data(), replaceLen, &written);

                                if (replaceLen < searchLen) {
                                    std::vector<BYTE> zeros(searchLen - replaceLen, 0);
                                    WriteProcessMemory(handle_,
                                        static_cast<BYTE*>(mbi.BaseAddress) + i + replaceLen,
                                        zeros.data(), zeros.size(), &written);
                                }

                                if (changedProtect)
                                    VirtualProtectEx(handle_, mbi.BaseAddress, mbi.RegionSize, oldProtect, &oldProtect);
                            }
                            ++count;
                        }
                    }
                }
            }
            current = static_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
        }
        return count;
    }

private:
    HANDLE handle_ = nullptr;
};

bool ControlThreads(DWORD pid, bool suspend) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    if (suspend)
                        SuspendThread(hThread);
                    else
                        ResumeThread(hThread);
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(snap, &te));
    }

    CloseHandle(snap);
    return true;
}

DWORD LaunchRoblox() {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;   // SW_HIDE yaparsan Roblox penceresi de gizlenir

    PROCESS_INFORMATION pi{};

    std::string cmd = R"("RobloxPlayerBeta.exe" -play -a "roblox://experiences/start?placeId=1")";
    std::vector<char> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return 0;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

void PatchClient(DWORD pid) {
    ProcessMemory mem(pid);
    if (!mem.IsValid()) return;

    constexpr int kThreshold = 9;
	constexpr int kMaxFound = 60;
    constexpr int kMinAttempts = 10;
    constexpr int kMaxAttempts = 20 + kMinAttempts;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (attempt <= kMinAttempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

		ControlThreads(pid, true);
        int found = mem.ReplaceString("roblox.com", "roblox.com", false);
        ControlThreads(pid, false);

		if (found >= kThreshold) {
            ControlThreads(pid, true);

            mem.ReplaceString("roblox.com", "rbolock.tk", true);

            ControlThreads(pid, false);
            return;
		}

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    DWORD pid = LaunchRoblox();
    if (!pid) return 1;

    PatchClient(pid);
    return 0;
}