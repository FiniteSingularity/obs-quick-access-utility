# --- Detect if the plugin is build out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
  if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
    legacy_check()
  endif()
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.18)
endif()

project(obs-quick-access-utility VERSION 0.0.1)
set(PROJECT_FULL_NAME "Quick Access Utility")

# Set new UUIDs when you start to create a new plugin.
set(MACOS_PACKAGE_UUID "0f0fa2d8-809c-462d-b600-24ab3788e4d0")
set(MACOS_INSTALLER_UUID "3dbf3525-1690-4d0f-a48d-226d4742c0e5")

add_library(${PROJECT_NAME} MODULE)

configure_file(src/version.h.in src/version.h)

target_sources(${PROJECT_NAME} PRIVATE
	src/plugin-module.c
	src/quick-access-utility.cpp
	src/quick-access-utility.hpp
	src/quick-access-dock.cpp
	src/quick-access-dock.hpp
	src/quick-access.cpp
	src/quick-access.hpp
	src/version.h)

if(BUILD_OUT_OF_TREE)
	find_package(libobs REQUIRED)
	find_package(obs-frontend-api REQUIRED)
	include(cmake/ObsPluginHelpers.cmake)
	set(OBS_FRONTEND_API_NAME "obs-frontend-api")
  else()
	  set(OBS_FRONTEND_API_NAME "frontend-api")
  endif()
  
if(OS_WINDOWS)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resource.rc.in ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
	target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.rc)
	
elseif(OS_MACOS)
	set(MACOS_BUNDLEID "com.finitesingularity.${PROJECT_NAME}")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
	configure_file(cmake/bundle/macos/installer-macos.pkgproj.in ${CMAKE_BINARY_DIR}/installer-macos.generated.pkgproj)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
elseif(OS_POSIX)
	target_link_libraries(${PROJECT_NAME} Qt::GuiPrivate)
endif()

# lines if you want add Qt UI in your plugin
find_qt(COMPONENTS Widgets COMPONENTS_LINUX Gui)
set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

target_link_libraries(${PROJECT_NAME}
		OBS::${OBS_FRONTEND_API_NAME}
		Qt::Widgets
		OBS::libobs)


if(BUILD_OUT_OF_TREE)
	if(NOT LIB_OUT_DIR)
		set(LIB_OUT_DIR "/lib/obs-plugins")
	endif()
	if(NOT DATA_OUT_DIR)
		set(DATA_OUT_DIR "/share/obs/obs-plugins/${PROJECT_NAME}")
	endif()
	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
	install(TARGETS ${PROJECT_NAME}
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
	install(DIRECTORY data/locale
		DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
		setup_plugin_target(${PROJECT_NAME})
else()
	target_include_directories(${PROJECT_NAME} PRIVATE
		"${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
	if(OBS_CMAKE_VERSION VERSION_GREATER_EQUAL 3.0.0)
		set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/finitesingularity" PREFIX "")
	else()
		set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "plugins/finitesingularity")
		setup_plugin_target(${PROJECT_NAME})
	endif()
endif()
