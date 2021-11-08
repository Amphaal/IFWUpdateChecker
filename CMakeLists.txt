cmake_minimum_required(VERSION 3.10.2)

project(IFWUpdateChecker
    VERSION 0.0.1
    DESCRIPTION "In-app simple helper to check if a Qt IFW package has updates"
    HOMEPAGE_URL "https://github.com/Amphaal/IFWUpdateChecker"
    LANGUAGES CXX
)

#cpp standards
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

##########################
## IFWUpdateChecker lib ##
##########################

add_library(IFWUpdateChecker INTERFACE)
target_include_directories(IFWUpdateChecker INTERFACE src)

###########################
## Deps : tinyProcessLib ##
###########################

# add
add_subdirectory(deps/tiny-process-library EXCLUDE_FROM_ALL)

#link
target_link_libraries(IFWUpdateChecker INTERFACE tiny-process-library::tiny-process-library)

if(WIN32)
    target_compile_definitions(tiny-process-library PUBLIC UNICODE)
endif()

#################################
## Deps : StupidHTTPDownloader ##
#################################

# add
add_subdirectory(deps/StupidHTTPDownloader)

# link
target_link_libraries(IFWUpdateChecker INTERFACE StupidHTTPDownloader)

###################
## Deps : spdlog ##
###################

# fetch
find_package(spdlog REQUIRED)

# link
target_link_libraries(IFWUpdateChecker INTERFACE spdlog::spdlog)

###########
## TESTS ##
###########

#dont build tests if included as submodule
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
