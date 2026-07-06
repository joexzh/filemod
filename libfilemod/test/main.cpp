#include <gtest/gtest.h>
#if _WIN32
#include <Windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
  setlocale(LC_ALL, ".UTF-8");
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#else
  setlocale(LC_ALL, "");  // trust the env on *nix system.
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}