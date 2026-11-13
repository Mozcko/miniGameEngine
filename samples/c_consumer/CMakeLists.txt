cmake_minimum_required(VERSION 3.24)
project(sample_c_consumer LANGUAGES C CXX)
enable_testing()

if(MGE_CONSUMER_FETCH)
    include(FetchContent)
    FetchContent_Declare(mge GIT_REPOSITORY https://github.com/Mozcko/miniGameEngine.git GIT_TAG v1.0.0)
    FetchContent_MakeAvailable(mge)
else()
    find_package(mge 1.0 REQUIRED)
endif()

mge_add_c_game(c_consumer SOURCES Main.c SMOKE_FRAMES 30)
