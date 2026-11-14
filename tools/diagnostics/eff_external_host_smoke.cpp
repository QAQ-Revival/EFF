#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "../../src/eiem_entry_settings.h"

// A harmless host used to verify path-scoped external injection without starting the game.
int wmain(int argc, wchar_t** argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  const bool absent = argc == 2 && std::wstring(argv[1]) == L"--expect-absent";
  for (unsigned i = 0; i < (absent ? 40u : 150u); ++i) {
    auto dll = GetModuleHandleW(L"eff.dll");
    if (dll) {
      if (absent) return 1;
      auto capability = reinterpret_cast<int(*)()>(GetProcAddress(dll, "EFF_ExternalData_v1"));
      if (!capability || capability() != 1) return 2;
      wchar_t file[32768]{}; GetModuleFileNameW(dll, file, 32768);
      const auto data = std::filesystem::path(file).parent_path();
      wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
      const auto game = std::filesystem::path(executable).parent_path();
      auto settings = EiemEntrySettings::Read(game, data);
      if (settings.reload.vk != VK_F10 || std::filesystem::exists(game/"plugin")) return 3;
      for (unsigned j = 0; j < 50; ++j) {
        std::ifstream log(data/"eff_log.txt", std::ios::binary);
        std::string text{std::istreambuf_iterator<char>(log), std::istreambuf_iterator<char>()};
        if (text.find("[BUILD] resource-directory-entry") != std::string::npos) ExitProcess(0);
        Sleep(100);
      }
      return 4;
    }
    Sleep(100);
  }
  return absent ? 0 : 5;
}
