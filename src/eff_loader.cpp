// QAQR's external EFF loader. No files are written to the game installation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <cstdint>

namespace fs = std::filesystem;
struct Handle {
  HANDLE value;
  explicit Handle(HANDLE v) : value(v) {}
  ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  operator HANDLE() const { return value; }
};
std::wstring Identity(const fs::path& path) {
  auto text = fs::weakly_canonical(fs::absolute(path)).wstring();
  if (text.rfind(L"\\\\?\\", 0) == 0) text.erase(0, 4);
  std::transform(text.begin(), text.end(), text.begin(), towlower);
  return text;
}
std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void Status(const fs::path& root, const std::string& request, const char* state, DWORD pid = 0, DWORD error = 0) {
  auto target = root / "loader-status.json";
  auto temporary = root / (".loader-status-" + std::to_string(GetCurrentProcessId()));
  { std::ofstream out(temporary, std::ios::binary); out << "{\"request\":\"" << request << "\",\"state\":\"" << state << "\",\"pid\":" << pid << ",\"error\":" << error << "}\n"; }
  if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("status write failed");
}
uintptr_t Module(DWORD pid, const fs::path& wanted, bool filenameOnly = false) {
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
  MODULEENTRY32W module{}; module.dwSize = sizeof(module);
  if (snapshot.value == INVALID_HANDLE_VALUE || !Module32FirstW(snapshot, &module)) return 0;
  do {
    bool match = filenameOnly ? _wcsicmp(module.szModule, wanted.filename().c_str()) == 0 : Identity(module.szExePath) == Identity(wanted);
    if (match) return reinterpret_cast<uintptr_t>(module.modBaseAddr);
  } while (Module32NextW(snapshot, &module));
  return 0;
}
DWORD Inject(HANDLE process, DWORD pid, const fs::path& dll) {
  if (Module(pid, dll)) return 0;
  auto load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  HMODULE owner = nullptr;
  if (!load || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(load), &owner)) return GetLastError();
  wchar_t ownerPath[32768]{};
  if (!GetModuleFileNameW(owner, ownerPath, 32768)) return GetLastError();
  const auto base = Module(pid, ownerPath, true);
  if (!base) return ERROR_MOD_NOT_FOUND;
  auto remoteLoad = reinterpret_cast<LPTHREAD_START_ROUTINE>(base + reinterpret_cast<uintptr_t>(load) - reinterpret_cast<uintptr_t>(owner));
  const auto wide = fs::absolute(dll).wstring();
  const auto bytes = (wide.size() + 1) * sizeof(wchar_t);
  auto memory = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!memory) return GetLastError();
  SIZE_T written = 0;
  if (!WriteProcessMemory(process, memory, wide.c_str(), bytes, &written) || written != bytes) {
    const auto error = GetLastError(); VirtualFreeEx(process, memory, 0, MEM_RELEASE); return error ? error : ERROR_WRITE_FAULT;
  }
  Handle thread(CreateRemoteThread(process, nullptr, 0, remoteLoad, memory, 0, nullptr));
  if (!thread.value) { const auto error = GetLastError(); VirtualFreeEx(process, memory, 0, MEM_RELEASE); return error; }
  auto waited = WaitForSingleObject(thread, 15000);
  // The remote thread may still be using its argument after a timeout.
  if (waited != WAIT_OBJECT_0) return waited == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
  VirtualFreeEx(process, memory, 0, MEM_RELEASE);
  for (unsigned i = 0; i < 10; ++i) {
    if (Module(pid, dll)) return ERROR_SUCCESS;
    Sleep(100);
  }
  return ERROR_DLL_INIT_FAILED;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  fs::path game, dll; std::string request;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (std::wstring(argv[i]) == L"--game") game = argv[i + 1];
    else if (std::wstring(argv[i]) == L"--dll") dll = argv[i + 1];
    else if (std::wstring(argv[i]) == L"--request") {
      std::wstring token = argv[i + 1];
      if (token.find_first_not_of(L"0123456789abcdef-") != std::wstring::npos) { LocalFree(argv); return ERROR_INVALID_PARAMETER; }
      for (auto c : token) request.push_back(static_cast<char>(c));
    }
    else { LocalFree(argv); return ERROR_INVALID_PARAMETER; }
  }
  LocalFree(argv);
  if (request.empty() || request.size() > 64 || request.find_first_not_of("0123456789abcdef-") != std::string::npos || !game.is_absolute() || !dll.is_absolute()) return ERROR_INVALID_PARAMETER;
  const auto root = dll.parent_path();
  try {
    if (!fs::is_regular_file(game) || !fs::is_regular_file(dll)) return ERROR_FILE_NOT_FOUND;
    Status(root, request, "waiting");
    const auto start = GetTickCount64();
    while (GetTickCount64() - start < 180000) {
      if (Read(root / ".launch-request") != request || fs::exists(root / "disabled")) return 0;
      Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
      PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
      if (snapshot.value == INVALID_HANDLE_VALUE || !Process32FirstW(snapshot, &entry)) throw std::runtime_error("process enumeration failed");
      do {
        if (_wcsicmp(entry.szExeFile, game.filename().c_str())) continue;
        Handle query(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        wchar_t executable[32768]{}; DWORD size = 32768;
        if (!query.value || !QueryFullProcessImageNameW(query, 0, executable, &size) || Identity(executable) != Identity(game)) continue;
        Handle process(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, entry.th32ProcessID));
        if (!process.value) { Status(root, request, "error", entry.th32ProcessID, GetLastError()); return 1; }
        if (Read(root / ".launch-request") != request || fs::exists(root / "disabled")) return 0;
        const auto error = Inject(process, entry.th32ProcessID, dll);
        // XXMI may still be starting a suspended process whose system modules are not ready.
        if (error == ERROR_MOD_NOT_FOUND) continue;
        Status(root, request, error ? "error" : "loaded", entry.th32ProcessID, error);
        return error ? 1 : 0;
      } while (Process32NextW(snapshot, &entry));
      Sleep(100);
    }
    Status(root, request, "timeout", 0, ERROR_TIMEOUT);
  } catch (...) { try { Status(root, request, "error", 0, ERROR_GEN_FAILURE); } catch (...) {} return 1; }
  return 1;
}
