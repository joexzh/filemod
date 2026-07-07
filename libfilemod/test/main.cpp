#include <gtest/gtest.h>
#if _WIN32
#include <Windows.h>
#endif

int main(int argc, char** argv) {
  setlocale(LC_ALL, "C.UTF-8");
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}