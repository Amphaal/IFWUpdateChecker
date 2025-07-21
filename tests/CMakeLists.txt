find_package(Catch2 REQUIRED)

add_executable(IFWUC_tests tests.cpp)

target_link_libraries(IFWUC_tests
    IFWUpdateChecker
    Catch2::Catch2WithMain
    spdlog::spdlog
)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${CATCH_SOURCE_DIR}/contrib)
include(Catch)
catch_discover_tests(IFWUC_tests)