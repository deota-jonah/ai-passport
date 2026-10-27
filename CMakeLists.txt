cmake_minimum_required(VERSION 3.16)
set(ENV{AGENT_LINK_BUILD_BACKEND} "WIFI")
set(PROJECT_VER "0.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(FoloToy-AI-Passport)
