#==================================================================================================#
#                                                                                                  #
#  Copyright 2012 MaidSafe.net limited                                                             #
#                                                                                                  #
#  This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,        #
#  version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which    #
#  licence you accepted on initial access to the Software (the "Licences").                        #
#                                                                                                  #
#  By contributing code to the MaidSafe Software, or to this project generally, you agree to be    #
#  bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root        #
#  directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available   #
#  at: http://www.maidsafe.net/licenses                                                            #
#                                                                                                  #
#  Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed    #
#  under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF   #
#  ANY KIND, either express or implied.                                                            #
#                                                                                                  #
#  See the Licences for the specific language governing permissions and limitations relating to    #
#  use of the MaidSafe Software.                                                                   #
#                                                                                                  #
#==================================================================================================#


set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/standard_setup.cmake")
  cmake_minimum_required(VERSION 2.8)  # To suppress warning cluttering error message
  set(Msg "\nThis project can currently only be build as part of the MaidSafe super-project.  For")
  set(Msg "${Msg} full details, see https://github.com/maidsafe/MaidSafe/wiki/Build-Instructions\n")
  message(FATAL_ERROR "${Msg}")
endif()

project(drive)

include(../../cmake_modules/standard_setup.cmake)


#==================================================================================================#
# Callback File System / Fuse library search                                                       #
#==================================================================================================#
if(WIN32)
  include(maidsafe_find_cbfs)
  if(NOT CbfsFound AND NOT DONT_USE_CBFS)
    set(Msg "\nFailed to find Callback File System.  This is a prerequisite for Drive on Windows, so ")
    set(Msg "${Msg}Drive will be excluded from the default build.  See ")
    set(Msg "${Msg}https://github.com/maidsafe/MaidSafe/wiki/Build-Instructions#optional-components")
    set(Msg "${Msg} for further info.\nIf Cbfs is already installed, run:\n")
    set(Msg "${Msg}cmake . -DCBFS_ROOT_DIR=<Path to Cbfs root directory>\n")
    set(Msg "${Msg}e.g.\ncmake . -DCBFS_ROOT_DIR=\"C:\\Program Files\\EldoS\\Callback File System\"\n")
    set(Msg "${Msg}To permanently disable this warning, run:\ncmake . -DDONT_USE_CBFS=TRUE\n")
    message(WARNING "${Msg}")
  endif()
else()
  include(maidsafe_find_fuse)
endif()


#==================================================================================================#
# Set up all files as GLOBs                                                                        #
#==================================================================================================#
set(DriveSourcesDir ${PROJECT_SOURCE_DIR}/src/maidsafe/drive)
set(DriveApiDir ${PROJECT_SOURCE_DIR}/include/maidsafe/drive)
ms_glob_dir(Drive ${DriveSourcesDir} Drive)
set(LocalDriveMain ${DriveSourcesDir}/local/local_drive.cc)
set(NetworkDriveMain ${DriveSourcesDir}/network/network_drive.cc)
set(WinServiceMain ${DriveSourcesDir}/win_service/win_service_main.cc)
set(UnixFiles ${DriveApiDir}/unix_drive.h ${DriveSourcesDir}/unix_drive.cc)
set(WinFiles ${DriveApiDir}/win_drive.h ${DriveSourcesDir}/win_drive.cc)
set(UnixFileCommands ${DriveSourcesDir}/tools/commands/unix_file_commands.h
                     ${DriveSourcesDir}/tools/commands/unix_file_commands.cc)
set(WindowsFileCommands ${DriveSourcesDir}/tools/commands/windows_file_commands.h
                        ${DriveSourcesDir}/tools/commands/windows_file_commands.cc)
if(CbfsFound)
  set(CbfsKeyFile ${CMAKE_CURRENT_BINARY_DIR}/cbfs_key/maidsafe/drive/cbfs_key.h)
  configure_file("${DriveSourcesDir}/cbfs_key.h.in" "${CbfsKeyFile}" @ONLY)
  list(APPEND DriveAllFiles ${CbfsKeyFile})
endif()

if(WIN32)
  list(REMOVE_ITEM DriveAllFiles ${UnixFiles})
else()
  list(REMOVE_ITEM DriveAllFiles ${WinFiles})
endif()

ms_glob_dir(DriveTests ${DriveSourcesDir}/tests Tests)
ms_glob_dir(DriveTools ${DriveSourcesDir}/tools Tools)
set(DriveLauncherFiles ${DriveApiDir}/tools/launcher.h ${DriveSourcesDir}/tools/launcher.cc)
set(SafeStorageFiles ${DriveApiDir}/tools/safe_storage.cc)
list(REMOVE_ITEM DriveToolsAllFiles ${DriveLauncherFiles})
list(REMOVE_ITEM DriveToolsAllFiles ${SafeStorageFiles})
ms_glob_dir(DriveToolsCommands ${DriveSourcesDir}/tools/commands Commands)
list(REMOVE_ITEM DriveToolsCommandsAllFiles ${UnixFileCommands} ${WindowsFileCommands})
if(WIN32)
  ms_glob_dir(Installer ${DriveSourcesDir}/installer Installer)
endif()
set(DriveTestsResourcesDir ${DriveSourcesDir}/tests/resources)


#==================================================================================================#
# Define MaidSafe libraries and executables                                                        #
#==================================================================================================#
ms_add_static_library(maidsafe_drive ${DriveAllFiles})
target_include_directories(maidsafe_drive
  PUBLIC ${PROJECT_SOURCE_DIR}/include ${CbfsIncludeDir} ${Fuse_INCLUDE_DIR}
  PRIVATE ${PROJECT_SOURCE_DIR}/src $<$<BOOL:${CbfsFound}>:${CMAKE_CURRENT_BINARY_DIR}/cbfs_key>)
if(WIN32)
  if(CbfsFound)
    ms_add_executable(cbfs_driver "Production" ${InstallerAllFiles})
    target_include_directories(cbfs_driver PRIVATE ${BoostSourceDir} ${CbfsIncludeDir})
    target_link_libraries(maidsafe_drive ${CbfsLibraries})
    target_link_libraries(cbfs_driver maidsafe_common ${CbfsLibraries})
  endif()
elseif(APPLE)
  target_link_libraries(maidsafe_drive ${Fuse_LIBRARY})
elseif(BSD)
  target_link_libraries(maidsafe_drive ${Fuse_LIBRARY})
else()
  target_link_libraries(maidsafe_drive ${Fuse_LIBRARY} rt dl)
endif()

ms_add_static_library(maidsafe_drive_launcher ${DriveLauncherFiles})
target_include_directories(maidsafe_drive_launcher PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(maidsafe_drive maidsafe_drive_launcher maidsafe_encrypt)
target_link_libraries(maidsafe_drive_launcher maidsafe_common maidsafe_nfs_client)

ms_add_executable(local_drive "Production" ${LocalDriveMain})
target_link_libraries(local_drive maidsafe_drive maidsafe_drive_launcher)
ms_add_executable(network_drive "Production" ${NetworkDriveMain})
target_link_libraries(network_drive maidsafe_drive maidsafe_drive_launcher maidsafe_nfs_client)
ms_add_executable(safe_storage "Tools/Drive" ${DriveSourcesDir}/tools/safe_storage.cc)
target_include_directories(safe_storage PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(safe_storage maidsafe_drive_launcher)

if(WIN32)
  set_property(TARGET local_drive network_drive PROPERTY WIN32_EXECUTABLE TRUE)
  target_compile_options(network_drive PUBLIC $<$<CONFIG:Debug>: /bigobj>)
  if(MaidsafeTesting)
    ms_add_executable(local_drive_console "Production" ${LocalDriveMain})
    target_link_libraries(local_drive_console maidsafe_drive maidsafe_drive_launcher)
    ms_add_executable(network_drive_console "Production" ${NetworkDriveMain})
    target_link_libraries(network_drive_console maidsafe_drive maidsafe_drive_launcher maidsafe_nfs_client)
    target_compile_options(network_drive_console PUBLIC $<$<CONFIG:Debug>: /bigobj>)
  endif()
endif()

add_dependencies(safe_storage network_drive)
if(WIN32)
  add_dependencies(safe_storage network_drive_console)
endif()

if(MaidsafeTesting)
  ms_add_executable(filesystem_weekly "Tools/Drive"
                    ${DriveSourcesDir}/tools/filesystem_weekly.cc
                    ${DriveSourcesDir}/tools/tool_main.cc)
  if(WIN32)
    ms_add_executable(filesystem_test "Tools/Drive"
                      ${DriveSourcesDir}/tools/filesystem_test.cc
                      ${DriveSourcesDir}/tools/tool_main.cc
                      ${WindowsFileCommands})
  else()
    ms_add_executable(filesystem_test "Tools/Drive"
                      ${DriveSourcesDir}/tools/filesystem_test.cc
                      ${DriveSourcesDir}/tools/tool_main.cc
                      ${UnixFileCommands})
  endif()
  ms_add_executable(filesystem_commands "Tools/Drive"
                    ${DriveSourcesDir}/tools/filesystem_commands.h
                    ${DriveSourcesDir}/tools/filesystem_commands.cc
                    ${DriveSourcesDir}/tools/tool_main.cc
                    ${DriveToolsCommandsAllFiles})
  ms_add_executable(test_drive "Tests/Drive" ${DriveTestsAllFiles})
  target_include_directories(test_drive PRIVATE ${PROJECT_SOURCE_DIR}/src)

  add_dependencies(filesystem_weekly local_drive network_drive)
  add_dependencies(filesystem_test local_drive network_drive)
  add_dependencies(filesystem_commands local_drive network_drive)
  if(WIN32)
    add_dependencies(filesystem_weekly local_drive_console network_drive_console)
    add_dependencies(filesystem_test local_drive_console network_drive_console)
    add_dependencies(filesystem_commands local_drive_console network_drive_console)
  endif()
  target_include_directories(filesystem_weekly PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(filesystem_test PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(filesystem_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_include_directories(filesystem_commands PRIVATE ${PROJECT_SOURCE_DIR}/src)

  target_link_libraries(filesystem_weekly maidsafe_drive_launcher)
  target_link_libraries(filesystem_test maidsafe_drive_launcher)
  target_link_libraries(filesystem_commands maidsafe_drive_launcher maidsafe_drive)
  add_dependencies(filesystem_test catch)
  target_link_libraries(test_drive maidsafe_drive)
endif()

ms_rename_outdated_built_exes()

if(WIN32 AND NOT CbfsFound)
  foreach(Target maidsafe_drive cbfs_driver local_drive network_drive local_drive_console network_drive_console filesystem_weekly filesystem_test filesystem_commands test_drive)
    if(TARGET ${Target})
      set_target_properties(${Target} PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
    endif()
  endforeach()
endif()


#==================================================================================================#
# Set compiler and linker flags                                                                    #
#==================================================================================================#
include(standard_flags)

target_compile_definitions(maidsafe_drive PUBLIC $<$<BOOL:${UNIX}>:FUSE_USE_VERSION=26>)
target_compile_definitions(local_drive PRIVATE $<$<BOOL:${WIN32}>:USES_WINMAIN>)
target_compile_definitions(network_drive PRIVATE $<$<BOOL:${WIN32}>:USES_WINMAIN>)

target_compile_definitions(safe_storage PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_COMPILE_DEFINITIONS>)
set_property(TARGET safe_storage APPEND PROPERTY COMPILE_DEFINITIONS "CMAKE_GENERATOR=\"${CMAKE_GENERATOR}\"")
if(WIN32)
  if(CMAKE_CL_64)
    set_property(TARGET safe_storage APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86_amd64")
  else()
    set_property(TARGET safe_storage APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86")
  endif()
endif()

if(MaidsafeTesting)
  target_compile_definitions(filesystem_weekly PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_COMPILE_DEFINITIONS>)
  target_compile_definitions(filesystem_test PRIVATE $<TARGET_PROPERTY:maidsafe_drive,INTERFACE_COMPILE_DEFINITIONS>)
  set_property(TARGET filesystem_weekly APPEND PROPERTY COMPILE_DEFINITIONS "CMAKE_GENERATOR=\"${CMAKE_GENERATOR}\"")
  set_property(TARGET filesystem_test APPEND PROPERTY COMPILE_DEFINITIONS "CMAKE_GENERATOR=\"${CMAKE_GENERATOR}\"")
  set_property(TARGET filesystem_weekly APPEND PROPERTY COMPILE_DEFINITIONS "DRIVE_TESTS_RESOURCES=${DriveTestsResourcesDir}")
  set_property(TARGET filesystem_test APPEND PROPERTY COMPILE_DEFINITIONS "DRIVE_TESTS_RESOURCES=${DriveTestsResourcesDir}")
  if(WIN32)
    if(CMAKE_CL_64)
      set_property(TARGET filesystem_weekly APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86_amd64")
      set_property(TARGET filesystem_test APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86_amd64")
    else()
      set_property(TARGET filesystem_weekly APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86")
      set_property(TARGET filesystem_test APPEND PROPERTY COMPILE_DEFINITIONS "VS_DEV_CMD=\"$ENV{VCInstallDir}vcvarsall.bat\" x86")
    endif()
  endif()
endif()
if(CbfsFound)
  target_compile_definitions(cbfs_driver PRIVATE $<TARGET_PROPERTY:maidsafe_common,INTERFACE_COMPILE_DEFINITIONS>)
  target_compile_options(cbfs_driver PRIVATE $<TARGET_PROPERTY:maidsafe_common,INTERFACE_COMPILE_OPTIONS>)
  set_property(TARGET cbfs_driver APPEND_STRING PROPERTY LINK_FLAGS " /level='requireAdministrator' /uiAccess='false' ")
  set_property(TARGET cbfs_driver APPEND PROPERTY COMPILE_DEFINITIONS "CBFS_ROOT_DIR=${CBFS_ROOT_DIR}")
  set(ProductId "47584d51-4651-487a-534c-675242655154")
  set_property(TARGET local_drive APPEND PROPERTY COMPILE_DEFINITIONS "PRODUCT_ID=${ProductId}")
  set_property(TARGET network_drive APPEND PROPERTY COMPILE_DEFINITIONS "PRODUCT_ID=${ProductId}")
  if(MaidsafeTesting)
    set_property(TARGET local_drive_console APPEND PROPERTY COMPILE_DEFINITIONS "PRODUCT_ID=${ProductId}")
    set_property(TARGET network_drive_console APPEND PROPERTY COMPILE_DEFINITIONS "PRODUCT_ID=${ProductId}")
  endif()
endif()


#==================================================================================================#
# Tests                                                                                            #
#==================================================================================================#
if(MaidsafeTesting)
  ms_add_style_test()
  ms_add_catch_tests(test_drive)
  add_test(NAME "\"Real Disk Filesystem Test\""
           COMMAND filesystem_test --durations yes --warn NoAssertions --disk
                   --name "filesystem_test ($<CONFIGURATION> build)")
  if(WIN32)
    set(Console _console)
  endif()
  add_test(NAME "\"Local Drive Filesystem Test\""
           COMMAND filesystem_test --durations yes --warn NoAssertions --local${Console}
                   --name "filesystem_test ($<CONFIGURATION> build)")
  set(Timeout 120)
  ms_update_test_timeout(Timeout)
  set_tests_properties("\"Real Disk Filesystem Test\"" "\"Local Drive Filesystem Test\"" PROPERTIES
                       FAIL_REGULAR_EXPRESSION "No tests ran"
                       TIMEOUT ${Timeout}
                       LABELS "Drive;Filesystem")

  if(WEEKLY)
    add_test(NAME "\"Real Disk Weekly\""
             COMMAND filesystem_weekly --durations yes --warn NoAssertions --disk
                     --name "filesystem_weekly ($<CONFIGURATION> build)")
    add_test(NAME "\"Local Drive Weekly\""
             COMMAND filesystem_weekly --durations yes --warn NoAssertions --local${Console}
                     --name "filesystem_weekly ($<CONFIGURATION> build)")
    set(Timeout 10800)
    ms_update_test_timeout(Timeout)
    set_tests_properties("\"Real Disk Weekly\"" "\"Local Drive Weekly\"" PROPERTIES
                         FAIL_REGULAR_EXPRESSION "No tests ran"
                         TIMEOUT ${Timeout}
                         LABELS "Drive;Filesystem;Weekly")
  endif()

  ms_add_project_experimental()
  ms_test_summary_output()
endif()


#==================================================================================================#
# Package                                                                                          #
#==================================================================================================#
set(EXPORT_NAME maidsafe_drive)
install(TARGETS maidsafe_drive EXPORT ${EXPORT_NAME} ARCHIVE DESTINATION lib)
set(EXPORT_NAME maidsafe_drive_launcher)
install(TARGETS maidsafe_drive_launcher EXPORT ${EXPORT_NAME} ARCHIVE DESTINATION lib)
set(EXPORT_NAME network_drive)
install(TARGETS network_drive EXPORT ${EXPORT_NAME} RUNTIME DESTINATION bin)
set(EXPORT_NAME local_drive)
install(TARGETS local_drive EXPORT ${EXPORT_NAME} RUNTIME DESTINATION bin)
if(CbfsFound)
  install(TARGETS cbfs_driver EXPORT ${EXPORT_NAME} RUNTIME DESTINATION bin)
endif()

install(FILES ${DriveApi} DESTINATION include/maidsafe/drive)
install(FILES ${DriveToolsApi} DESTINATION include/maidsafe/drive/tools)
