cmake_minimum_required(VERSION 3.24)
project(sample_consumer LANGUAGES C CXX)
enable_testing()

if(MGE_CONSUMER_FETCH)
    include(FetchContent)
    FetchContent_Declare(mge GIT_REPOSITORY https://github.com/Mozcko/miniGameEngine.git GIT_TAG v0.9.0)
    FetchContent_MakeAvailable(mge)
else()
    find_package(mge 0.9 REQUIRED)
endif()

mge_add_game(consumer SOURCES Main.cpp SMOKE_FRAMES 30)
