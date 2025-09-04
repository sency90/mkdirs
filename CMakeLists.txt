cmake_minimum_required(VERSION 3.10)
project(mg_dirs CXX)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Executable: mg_dirs (release-ish flags) ---
add_executable(mg_dirs
  src/main.cpp
)
target_include_directories(mg_dirs PRIVATE src)
target_compile_options(mg_dirs PRIVATE -g)

# --- Tests on/off switch ---
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
  enable_testing()

  # Executable: mg_dirs_test
  add_executable(mg_dirs_test
    test/test_logic.cpp
    test/test_main.cpp
  )
  # 테스트 코드가 src의 헤더/심볼을 쓴다면 include 필요
  target_include_directories(mg_dirs_test PRIVATE src)

  # gtest/gmock 찾기 (패키지 설정이 있는 환경)
  find_package(GTest)
  if(GTest_FOUND)
    # 보통 최신 CMake 패키지는 아래와 같이 타깃을 노출함
    # (환경에 따라 GTest::gmock이 없을 수도 있으니 gmock도 체크)
    if(TARGET GTest::gmock)
      target_link_libraries(mg_dirs_test PRIVATE GTest::gtest GTest::gmock pthread)
    else()
      target_link_libraries(mg_dirs_test PRIVATE GTest::gtest pthread)
    endif()
  else()
    # 패키지 못 찾으면 수동 링크 (Ubuntu 빌드/설치 기준)
    # /usr/include/gmock/gmock.h, /usr/include/gtest/gtest.h
    # /usr/lib/libgmock.a, /usr/lib/libgtest.a
    target_link_libraries(mg_dirs_test PRIVATE gmock gtest pthread)
  endif()

  # 디버깅 용 플래그 (테스트만 -g)
  target_compile_options(mg_dirs_test PRIVATE -g)

  # ctest 등록
  add_test(NAME mg_dirs_test COMMAND mg_dirs_test)
endif()
