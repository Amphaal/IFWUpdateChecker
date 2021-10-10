cmake_minimum_required(VERSION 3.10.2)

project(IFWUpdateChecker
    VERSION 0.0.1
    DESCRIPTION "Check if an IFW package has updates"
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

#link
add_subdirectory(deps/tiny-process-library EXCLUDE_FROM_ALL)
target_link_libraries(IFWUpdateChecker INTERFACE tiny-process-library)

#########################################
## Find libraries upon which work with ##
#########################################

# for logging
if(NOT TARGET spdlog::spdlog)
    find_package(spdlog)
    if(NOT spdlog_FOUND)
        message("Including [spdlog] !")
            Include(FetchContent)
            FetchContent_Declare(spdlog
                GIT_REPOSITORY "https://github.com/gabime/spdlog"
                GIT_TAG "v1.x"
            )
            FetchContent_MakeAvailable(spdlog)
    endif()
endif()

# for network

#TODO

##########
## TESTS #
##########

#dont build tests if included as submodule
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests)
endif()