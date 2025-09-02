cmake_minimum_required(VERSION 3.10)
project(mkdirs CXX)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Executable: mkdirs (release-ish flags) ---
add_executable(mkdirs
  src/main.cpp
)
target_include_directories(mkdirs PRIVATE src)
target_compile_options(mkdirs PRIVATE -g)

# --- Tests on/off switch ---
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
  enable_testing()

  # Executable: mkdirs_test
  add_executable(mkdirs_test
    test/test_logic.cpp
    test/test_main.cpp
  )
  # 테스트 코드가 src의 헤더/심볼을 쓴다면 include 필요
  target_include_directories(mkdirs_test PRIVATE src)

  # gtest/gmock 찾기 (패키지 설정이 있는 환경)
  find_package(GTest)
  if(GTest_FOUND)
    # 보통 최신 CMake 패키지는 아래와 같이 타깃을 노출함
    # (환경에 따라 GTest::gmock이 없을 수도 있으니 gmock도 체크)
    if(TARGET GTest::gmock)
      target_link_libraries(mkdirs_test PRIVATE GTest::gtest GTest::gmock pthread)
    else()
      target_link_libraries(mkdirs_test PRIVATE GTest::gtest pthread)
    endif()
  else()
    # 패키지 못 찾으면 수동 링크 (Ubuntu 빌드/설치 기준)
    # /usr/include/gmock/gmock.h, /usr/include/gtest/gtest.h
    # /usr/lib/libgmock.a, /usr/lib/libgtest.a
    target_link_libraries(mkdirs_test PRIVATE gmock gtest pthread)
  endif()

  # 디버깅 용 플래그 (테스트만 -g)
  target_compile_options(mkdirs_test PRIVATE -g)

  # ctest 등록
  add_test(NAME mkdirs_test COMMAND mkdirs_test)
endif()
