## CMakeLists.txt — Tests unitaires CN105 (standalone, sans dépendance ESPHome)
## Role: Build system pour les tests de non-régression sur les fonctions pures du protocole CN105.
## Deps: CMake >= 3.14 (GoogleTest est téléchargé automatiquement via FetchContent)

cmake_minimum_required(VERSION 3.14)
project(cn105_unit_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── GoogleTest via FetchContent (portable: fonctionne en CI sans apt install) ──
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Source du composant (chemin relatif vers components/cn105)
set(CN105_SRC_DIR "${CMAKE_SOURCE_DIR}/../../components/cn105")

# Stubs ESPHome
include_directories(${CMAKE_SOURCE_DIR}/mocks)
# Headers du composant
include_directories(${CN105_SRC_DIR})

# --- Test executable ---
add_executable(cn105_tests
    test_checksum.cpp
    test_lookup_tables.cpp
    test_temperature.cpp
    test_types.cpp
    test_calculate_temp.cpp
)

target_link_libraries(cn105_tests
    GTest::gtest
    GTest::gtest_main
)

# Discovery des tests pour ctest
enable_testing()
include(GoogleTest)
gtest_discover_tests(cn105_tests)
