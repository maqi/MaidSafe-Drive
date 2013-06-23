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

#ifndef MAIDSAFE_DRIVE_DIRECTORY_LISTING_HANDLER_H_
#define MAIDSAFE_DRIVE_DIRECTORY_LISTING_HANDLER_H_

#include <deque>
#include <list>
#include <memory>
#include <string>
#include <map>
#include <utility>

#include "boost/filesystem/path.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/utils.h"

// #include "maidsafe/data_store/data_store.h"
#include "maidsafe/data_store/permanent_store.h"
#include "maidsafe/nfs/nfs.h"

#include "maidsafe/drive/config.h"
#include "maidsafe/drive/drive_api.h"
#include "maidsafe/drive/directory_listing.h"
#include "maidsafe/drive/utils.h"


namespace fs = boost::filesystem;
namespace bptime = boost::posix_time;

namespace maidsafe {

namespace drive {

namespace test { class DirectoryListingHandlerTest; }

const size_t kMaxAttempts = 3;

struct DirectoryData {
  DirectoryData(const DirectoryId& parent_id_in, DirectoryListingPtr dir_listing)
      : parent_id(parent_id_in),
        listing(dir_listing),
        last_save(boost::posix_time::microsec_clock::universal_time()),
        last_change(kMaidSafeEpoch),
        content_changed(false) {}
  DirectoryData()
      : parent_id(),
        listing(),
        last_save(boost::posix_time::microsec_clock::universal_time()),
        last_change(kMaidSafeEpoch),
        content_changed(false) {}
  DirectoryId parent_id;
  DirectoryListingPtr listing;
  bptime::ptime last_save, last_change;
  bool content_changed;
};

class DirectoryListingHandler {
 public:
  typedef nfs::ClientMaidNfs ClientNfs;
  // typedef data_store::DataStore<data_store::DataBuffer> DataStore;
  typedef data_store::PermanentStore DataStore;
  typedef passport::Maid Maid;
  typedef std::pair<DirectoryData, uint32_t> DirectoryType;
  typedef OwnerDirectory::name_type OwnerDirectoryNameType;
  typedef GroupDirectory::name_type GroupDirectoryNameType;
  typedef WorldDirectory::name_type WorldDirectoryNameType;
  typedef OwnerDirectory::serialised_type OwnerDirectorySerialisedType;
  typedef GroupDirectory::serialised_type GroupDirectorySerialisedType;
  typedef WorldDirectory::serialised_type WorldDirectorySerialisedType;

  enum { kOwnerValue, kGroupValue, kWorldValue, kInvalidValue };

  DirectoryListingHandler(ClientNfs& client_nfs,
                          DataStore& data_store,
                          const Maid& maid,
                          const Identity& unique_user_id,
                          std::string root_parent_id);
  // Virtual destructor to allow inheritance in testing
  virtual ~DirectoryListingHandler();

  Identity unique_user_id() const { return unique_user_id_; }
  Identity root_parent_id() const { return root_parent_id_; }
  DirectoryType GetFromPath(const fs::path& relative_path);
  // Adds a directory or file represented by meta_data and relative_path to the appropriate parent
  // directory listing.  If the element is a directory, a new directory listing is created and
  // stored.  The parent directory's ID is returned in parent_id and its parent directory's ID is
  // returned in grandparent_id.
  void AddElement(const fs::path& relative_path,
                  const MetaData& meta_data,
                  DirectoryId* grandparent_id,
                  DirectoryId* parent_id);
  // Deletes the directory or file represented by relative_path from the
  // appropriate parent directory listing.  If the element is a directory, its
  // directory listing is deleted.  meta_data is filled with the element's
  // details if found.  If the element is a file, this allows the caller to
  // delete any corresponding chunks.  If save_changes is true, the parent
  // directory listing is stored after the deletion.
  void DeleteElement(const fs::path& relative_path, MetaData& meta_data);
  bool CanDelete(const fs::path& relative_path);
  void RenameElement(const fs::path& old_relative_path,
                     const fs::path& new_relative_path,
                     MetaData& meta_data,
                     int64_t& reclaimed_space);
  void UpdateParentDirectoryListing(const fs::path& parent_path, MetaData meta_data);

  int MoveDirectory(const fs::path& from, MetaData& from_meta_data, const fs::path& to);
  void SetWorldReadWrite();
  void SetWorldReadOnly();

  ClientNfs& client_nfs() const { return client_nfs_; }
  DataStore& data_store() const { return data_store_; }

  friend class test::DirectoryListingHandlerTest;

 protected:
  DirectoryListingHandler(const DirectoryListingHandler&);
  DirectoryListingHandler& operator=(const DirectoryListingHandler&);
  DirectoryData RetrieveFromStorage(const DirectoryId& parent_id,
                                    const DirectoryId& directory_id,
                                    int directory_type) const;
  void PutToStorage(const DirectoryType& directory);
  void DeleteStored(const DirectoryId& parent_id,
                    const DirectoryId& directory_id,
                    int directory_type);
  void RetrieveDataMap(const DirectoryId& parent_id,
                       const DirectoryId& directory_id,
                       int directory_type,
                       DataMapPtr data_map) const;
  bool IsDirectory(const MetaData& meta_data) const;
  void GetParentAndGrandparent(const fs::path& relative_path,
                               DirectoryType* grandparent,
                               DirectoryType* parent,
                               MetaData* parent_meta_data);
  bool RenameTargetCanBeRemoved(const fs::path& new_relative_path,
                                const MetaData& target_meta_data);
  int GetDirectoryType(const fs::path& relative_path);
  bool CanAdd(const fs::path& relative_path);
  bool CanRename(const fs::path& from_path, const fs::path& to_path);
  void RenameSameParent(const fs::path& old_relative_path,
                        const fs::path& new_relative_path,
                        MetaData& meta_data,
                        int64_t& reclaimed_space);
  void RenameDifferentParent(const fs::path& old_relative_path,
                             const fs::path& new_relative_path,
                             MetaData& meta_data,
                             int64_t& reclaimed_space);
  void ReStoreDirectories(const fs::path& relative_path, int directory_type);

 private:
  ClientNfs& client_nfs_;
  DataStore& data_store_;
  const Maid kMaid_;
  Identity unique_user_id_, root_parent_id_;
  fs::path relative_root_;
  bool world_is_writeable_;
};

}  // namespace drive

}  // namespace maidsafe

#endif  // MAIDSAFE_DRIVE_DIRECTORY_LISTING_HANDLER_H_