cmake_minimum_required(VERSION 4.0)
project(
    blredirect
    VERSION 1.0
    LANGUAGES CXX C
)

################################################################################
# | DEPENDENCIES |
################################################################################

include(FetchContent)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(minhook)
set(CMAKE_SKIP_INSTALL_RULES OFF)

set(LIBHAT_INSTALL_TARGET OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libhat
    GIT_REPOSITORY https://github.com/BasedInc/libhat.git
    GIT_TAG v0.11.1
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(libhat)

################################################################################
# | BLREDIRECT |
################################################################################

file(GLOB_RECURSE BLREDIRECT_SOURCE_LIST CONFIGURE_DEPENDS src/blredirect/*.cpp src/blredirect/*.h)

add_library(blredirect SHARED ${BLREDIRECT_SOURCE_LIST})
target_include_directories(blredirect PRIVATE src)
target_compile_features(blredirect PRIVATE cxx_std_20)
target_link_libraries(blredirect PRIVATE minhook libhat::libhat)

if(MSVC)
    target_compile_options(blredirect PRIVATE /W4 /EHsc /utf-8)
endif()

# clean before install - only do this when installing into out/install
install(CODE "
    if(CMAKE_INSTALL_PREFIX MATCHES \"/out/install/\")
        file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}\")
    endif()
")

install(TARGETS blredirect RUNTIME DESTINATION .)

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:blredirect> DESTINATION . OPTIONAL)
endif()
