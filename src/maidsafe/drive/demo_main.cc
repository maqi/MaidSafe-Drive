/*******************************************************************************
 *  Copyright 2011 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 *******************************************************************************
 */

#ifdef WIN32
#  include <conio.h>
#endif

#include <functional>
#include <iostream>  // NOLINT
#include <memory>
#include <string>
#include <fstream>  // NOLINT
#include <iterator>

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"
#include "boost/preprocessor/stringize.hpp"
#include "boost/system/error_code.hpp"

#include "maidsafe/common/crypto.h"
#include "maidsafe/common/log.h"
#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/data_store/data_store.h"
#include "maidsafe/nfs/nfs.h"

#include "maidsafe/encrypt/self_encryptor.h"

#ifdef WIN32
#  include "maidsafe/drive/win_drive.h"
#else
#  include "maidsafe/drive/unix_drive.h"
#endif
#include "maidsafe/drive/return_codes.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace maidsafe {

namespace drive {

#ifdef WIN32
typedef CbfsDriveInUserSpace DemoDriveInUserSpace;
#else
typedef FuseDriveInUserSpace DemoDriveInUserSpace;
#endif

typedef std::shared_ptr<DemoDriveInUserSpace> DemoDriveInUserSpacePtr;


int Mount(const fs::path &mount_dir,
          const fs::path &chunk_dir,
          const passport::Maid& maid,
          bool first_run) {
  fs::path data_store_path(chunk_dir / RandomAlphaNumericString(8));
  DiskUsage disk_usage(1048576000);
  MemoryUsage memory_usage(0);
  /*data_store::DataStore<data_store::DataBuffer> data_store(memory_usage,
                                                           disk_usage,
                                                           data_store::DataBuffer::PopFunctor(),
                                                           data_store_path);*/
  data_store::PermanentStore data_store(data_store_path, disk_usage);
  routing::Routing routing(maid);
  nfs::ClientMaidNfs client_nfs(routing, maid);

  boost::system::error_code error_code;
  if (!fs::exists(chunk_dir, error_code))
    return error_code.value();

  std::string root_parent_id;
  fs::path id_path(chunk_dir / "root_parent_id");
  if (!first_run)
    BOOST_VERIFY(ReadFile(id_path, &root_parent_id) && !root_parent_id.empty());

  // The following values need to be passed in and returned on unmount...
  int64_t max_space(std::numeric_limits<int64_t>::max()), used_space(0);
  Identity unique_user_id(Identity(std::string(64, 'a')));
  DemoDriveInUserSpacePtr drive_in_user_space(std::make_shared<DemoDriveInUserSpace>(
                                                  client_nfs,
                                                  data_store,
                                                  maid,
                                                  unique_user_id,
                                                  root_parent_id,
                                                  mount_dir,
                                                  "MaidSafeDrive",
                                                  max_space,
                                                  used_space));
  if (first_run)
    BOOST_VERIFY(WriteFile(id_path, drive_in_user_space->root_parent_id()));

#ifdef WIN32
  // drive_in_user_space->WaitUntilUnMounted();
  while (!kbhit());
  drive_in_user_space->Unmount(max_space, used_space);
#endif

  return 0;
}

}  // namespace drive

}  // namespace maidsafe


fs::path GetPathFromProgramOption(const std::string &option_name,
                                  po::variables_map *variables_map,
                                  bool must_exist) {
  if (variables_map->count(option_name)) {
    boost::system::error_code error_code;
    fs::path option_path(variables_map->at(option_name).as<std::string>());
    if (must_exist) {
      if (!fs::exists(option_path, error_code) || error_code) {
        LOG(kError) << "Invalid " << option_name << " option.  " << option_path
                    << " doesn't exist or can't be accessed (error message: "
                    << error_code.message() << ")";
        return fs::path();
      }
      if (!fs::is_directory(option_path, error_code) || error_code) {
        LOG(kError) << "Invalid " << option_name << " option.  " << option_path
                    << " is not a directory (error message: "
                    << error_code.message() << ")";
        return fs::path();
      }
    } else {
      if (fs::exists(option_path, error_code)) {
        LOG(kError) << "Invalid " << option_name << " option.  " << option_path
                    << " already exists (error message: "
                    << error_code.message() << ")";
        return fs::path();
      }
    }
    LOG(kInfo) << option_name << " set to " << option_path;
    return option_path;
  } else {
    LOG(kWarning) << "You must set the " << option_name << " option to a"
                 << (must_exist ? "n " : " non-") << "existing directory.";
    return fs::path();
  }
}


int main(int argc, char *argv[]) {
  maidsafe::log::Logging::Instance().Initialise(argc, argv);
  // Logfiles are written into this directory instead of the default logging one
//  FLAGS_log_dir = boost::filesystem::temp_directory_path().string();
  boost::system::error_code error_code;
#ifdef WIN32
  fs::path logging_dir("C:\\ProgramData\\Sigmoid\\Core\\logs");
#else
  fs::path logging_dir(fs::temp_directory_path(error_code) / "sigmoid/logs");
  if (error_code) {
    LOG(kError) << error_code.message();
    return 1;
  }
#endif
  if (!fs::exists(logging_dir, error_code))
    fs::create_directories(logging_dir, error_code);
  if (error_code)
    LOG(kError) << error_code.message();
  if (!fs::exists(logging_dir, error_code))
    LOG(kError) << "Couldn't create logging directory at " << logging_dir;
  fs::path log_path(logging_dir / "sigmoid_core_");
//  for (google::LogSeverity s = google::WARNING; s < google::NUM_SEVERITIES; ++s)
//    google::SetLogDestination(s, "");
//  google::SetLogDestination(google::INFO, log_path.string().c_str());

  // To set a config file option (only done by hand) do something like this:
  // mountdir=/drive/name/
  //    or
  // mountdir=S:
  // All command line parameters are only for this run.  To allow persistance,
  // update the config file.  Command line overrides any config file settings.
  try {
    po::options_description options_description("Allowed options");
    options_description.add_options()
        ("help,H", "print this help message")
        ("chunkdir,C", po::value<std::string>(),
         "set directory to store chunks")
        ("metadatadir,M", po::value<std::string>(),
         "set directory to store metadata")
        ("mountdir,D", po::value<std::string>(), "set virtual drive name")
        ("checkdata", "check all data (metadata and chunks)")
        ("start", "start Sigmoid Core (mount drive) [default]")  // daemonise
        ("stop", "stop Sigmoid Core (unmount drive) [not implemented]");
    // dunno if we can from here!

    po::variables_map variables_map;
    po::store(po::command_line_parser(argc, argv).options(options_description).allow_unregistered().
                                                  run(), variables_map);
    po::notify(variables_map);

    // set up options for config file
    po::options_description config_file_options;
    config_file_options.add(options_description);

    // try open some config options
    std::ifstream local_config_file("sigmoid_core.conf");
#ifdef WIN32
    fs::path main_config_path("C:/ProgramData/Sigmoid/Core/sigmoid_core.conf");
#else
    fs::path main_config_path("/etc/sigmoid_core.conf");
#endif
    std::ifstream main_config_file(main_config_path.string().c_str());

    // try local first for testing
    if (local_config_file) {
      LOG(kInfo) << "Using local config file \"sigmoid_core.conf\"";
      store(parse_config_file(local_config_file, config_file_options), variables_map);
      notify(variables_map);
    } else if (main_config_file) {
      LOG(kInfo) << "Using main config file " << main_config_path;
      store(parse_config_file(main_config_file, config_file_options), variables_map);
      notify(variables_map);
    } else {
      LOG(kWarning) << "No configuration file found at " << main_config_path;
    }

    if (variables_map.count("help")) {
      LOG(kInfo) << options_description;
      return 1;
    }

    fs::path chunkstore_path(GetPathFromProgramOption("chunkdir", &variables_map, true));
#ifdef WIN32
    fs::path mount_path(GetPathFromProgramOption("mountdir", &variables_map, false));
#else
    fs::path mount_path(GetPathFromProgramOption("mountdir", &variables_map, true));
#endif

    if (variables_map.count("stop")) {
      LOG(kInfo) << "Trying to stop.";
      return 0;
    }

    if (chunkstore_path == fs::path() || mount_path == fs::path()) {
      LOG(kWarning) << options_description;
      return 1;
    }

    fs::path maid_path(chunkstore_path / "serialised.maid");
    bool first_run(!fs::exists(maid_path, error_code));

    maidsafe::passport::Maid::signer_type maid_signer;
    maidsafe::passport::Maid maid(maid_signer);

    /*if (first_run) {
      fob = maidsafe::priv::utils::GenerateFob(nullptr);
      maidsafe::NonEmptyString serialised_fob(maidsafe::priv::utils::SerialiseFob(fob));
      BOOST_VERIFY(maidsafe::WriteFile(fob_path, serialised_fob.string()));
    } else {
      std::string str_serialised_fob;
      BOOST_VERIFY(maidsafe::ReadFile(fob_path, &str_serialised_fob));
      fob = maidsafe::priv::utils::ParseFob(maidsafe::NonEmptyString(str_serialised_fob));
    }*/

    int result(maidsafe::drive::Mount(mount_path,
                                      chunkstore_path,
                                      maid,
                                      first_run));
    return result;
  }
  catch(const std::exception& e) {
    LOG(kError) << "Exception: " << e.what();
    return 1;
  }
  catch(...) {
    LOG(kError) << "Exception of unknown type!";
  }
  return 0;
}