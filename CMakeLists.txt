cmake_minimum_required(VERSION 3.16...3.26)

# probably best to move this to a seperate file
function(setup_obs_lib_dependency target)
  if(BUILD_OUT_OF_TREE)
    find_package(libobs)
    if(libobs_FOUND AND NOT LIBOBS_LIB)
      target_link_libraries(${target} PUBLIC OBS::libobs)
    else()
      if(NOT LIBOBS_LIB)
        message(FATAL_ERROR "obs library not found - please set LIBOBS_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_LIB})
      if(NOT LIBOBS_INCLUDE_DIR)
        message(FATAL_ERROR "obs.hpp header not found - please set LIBOBS_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_INCLUDE_DIR})
    endif()
    find_package(obs-frontend-api)
    if(obs-frontend-api_FOUND AND NOT LIBOBS_FRONTEND_API_LIB)
      target_link_libraries(${target} PUBLIC OBS::obs-frontend-api)
    else()
      if(NOT LIBOBS_FRONTEND_API_LIB)
        message(FATAL_ERROR "libobs frontend-api library not found - please set LIBOBS_FRONTEND_API_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_FRONTEND_API_LIB})
      if(NOT LIBOBS_FRONTEND_INCLUDE_DIR)
        message(FATAL_ERROR " obs-frontend-api.h not found - please set LIBOBS_FRONTEND_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_FRONTEND_INCLUDE_DIR})
    endif()
  else()
    target_link_libraries(${target} PUBLIC OBS::libobs OBS::frontend-api)
  endif()
endfunction()

#

project(obs-quick-access-utility VERSION 1.0.4)

message(STATUS "CMAKE_PROJECT_NAME is ${CMAKE_PROJECT_NAME}")
if(${CMAKE_PROJECT_NAME} STREQUAL "obs-studio")
  set(OBS_FRONTEND_API_NAME "frontend-api")
  if(NOT DEFINED BUILD_OUT_OF_TREE)
    message(STATUS "${PROJECT_NAME} configured for in-tree build")
  endif()
else()
  set(BUILD_OUT_OF_TREE ON)
  set(OBS_FRONTEND_API_NAME "obs-frontend-api")
  message(STATUS "${PROJECT_NAME} configured for out-of-tree build")
endif()

if(BUILD_OUT_OF_TREE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
  include(compilerconfig)
  include(defaults)
  include(helpers)
endif()

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

add_library(${PROJECT_NAME} MODULE)

if(BUILD_OUT_OF_TREE)
  find_package(${OBS_FRONTEND_API_NAME} REQUIRED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::${OBS_FRONTEND_API_NAME})

find_package(Qt6 COMPONENTS Widgets Core)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

target_sources(
  ${PROJECT_NAME}
  PRIVATE src/plugin-module.c
          src/quick-access-utility.cpp
          src/quick-access-utility.hpp
          src/quick-access-dock.cpp
          src/quick-access-dock.hpp
          src/quick-access.cpp
          src/quick-access.hpp
          src/quick-access-source.cpp
          src/quick-access-source.hpp
          src/version.h)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

setup_obs_lib_dependency(${PROJECT_NAME})
if(BUILD_OUT_OF_TREE)
  set_target_properties_plugin(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
else()
  set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/finitesingularity" PREFIX "")
endif()

if(BUILD_OUT_OF_TREE)
  if(OS_WINDOWS)
    set(OBS_PLUGIN_DESTINATION "obs-plugins/64bit")
  else()
    set(OBS_PLUGIN_DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
  endif()
endif()
