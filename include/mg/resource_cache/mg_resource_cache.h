//**************************************************************************************************
// Mg Engine
//--------------------------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_resource_cache.h
 * Manages loading and updating of data resources, acting as a in-memory cache to the file-system.
 */

#pragma once

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_identifier.h"
#include "mg/resource_cache/internal/mg_resource_entry.h"
#include "mg/resource_cache/mg_resource_access_guard.h"
#include "mg/resource_cache/mg_resource_handle.h"
#include "mg/resources/mg_file_changed_event.h"
#include "mg/utils/mg_macros.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace Mg {

class ResourceCache;

/** ResourceCache is an efficient and flexible way of loading and using resources.
 * It works with both file-system directories and zip archives via file loaders (see IFileLoader).
 *
 * When resource files are requested, the cache is checked to see if the resource is already loaded.
 * If not (cache miss), then the resource is loaded using the cache's resource loaders.
 *
 * If a file is available in multiple file loaders, then the file with the greater time stamp is
 * used.
 *
 * While archive files are more efficient, it is easier to edit files in directories. However,
 * assets that are finished should be included in an archive, instead, for better performance (as
 * less fragmented resource data reduces hard-drive seek time).
 *
 * The cache maintains an list of files available to its resource loaders. This allows the cache to
 * know whether to load from directory or from archive, without a file system look-up. However, it
 * also means that `refresh()` should be called if either directory or archive contents have
 * changed. One may, for example, call `refresh()` upon window-receiving-focus events.
 */
class ResourceCache {
public:
    /** Construct ResourceCache with the given file loaders to use to find and load files.
     *
     * @param file_loaders The file loaders that this ResourceCache should use to find and load
     * files -- each loader representing e.g. a directory or a zip archive. Type: arbitrary number
     * of std::unique_ptr to type derived from IFileLoader.
     *
     * Usage example, creating a ResourceCache that loads files from a zip archive:
     *
     *     ResourceCache cache{ std::make_unique<ZipFileLoader>("data/data.zip") };
     *
     * In this case, the path to archive is given relative to current working directory.
     */
    template<typename... LoaderTs> explicit ResourceCache(std::unique_ptr<LoaderTs>... file_loaders)
    {
        MG_ASSERT((... && (file_loaders != nullptr)) && "File loaders may not be nullptr.");

        // This assertion is redundant but gives a nicer error message on type errors.
        static_assert((... && std::is_base_of_v<IFileLoader, LoaderTs>),
                      "ResourceCache constructor: arguments passed in file_loaders... must be "
                      "std::unique_ptr to a type derived from IFileLoader.");

        static_assert(sizeof...(file_loaders) > 0,
                      "ResourceCache constructor: there must be at least one file loader.");

        (m_file_loaders.push_back(std::move(file_loaders)), ...);
        refresh();
    }

    MG_MAKE_NON_COPYABLE(ResourceCache);
    MG_MAKE_NON_MOVABLE(ResourceCache); // Prevents pointer invalidation

    /** Update file list, detects if files have changed (added, removed, changed timestamp). */
    void refresh();

    /** Get handle to a resource with the given path.
     * @param file Filename (path) to resource file.
     * @load_resource_immediately Whether to load the resource into the cache before returning the
     * handle, or to defer loading until first access.
     */
    template<typename ResT>
    ResourceHandle<ResT> resource_handle(Identifier file, bool load_resource_immediately = true)
    {
        ResourceHandle<ResT> handle;

        {
            std::shared_lock lock{ m_file_list_mutex };

            FileInfo* p_file_info = file_info(file);
            if (!p_file_info) { throw_resource_not_found(file); }

            ResourceEntryBase& entry = get_or_create_resource_entry<ResT>(*p_file_info);
            handle = ResourceHandle<ResT>(file, static_cast<ResourceEntry<ResT>&>(entry));
        }

        if (load_resource_immediately) { ResourceAccessGuard access(handle); }

        return handle;
    }

    /** Access the resource with the given file path.
     * @param file Filename (path) to resource file.
     */
    template<typename ResT> ResourceAccessGuard<ResT> access_resource(Identifier file)
    {
        return ResourceAccessGuard(resource_handle<ResT>(file));
    }

    /** Returns whether a file with the given path exists in the file list.
     * N.B. returns the state as of most recent call to `refresh()`
     */
    bool file_exists(Identifier file) const
    {
        std::shared_lock lock{ m_file_list_mutex };
        return file_info(file) != nullptr;
    }

    /** Returns the time stamp of the given file. Throws if file does not exist in file list. */
    time_point file_time_stamp(Identifier file) const
    {
        std::shared_lock lock{ m_file_list_mutex };
        auto             p_file_info = file_info(file);
        if (!p_file_info) { throw_resource_not_found(file); }
        return p_file_info->time_stamp;
    }

    /** Returns whether the resource with given id is currently cached in this ResourceCache. */
    bool is_cached(Identifier resource_id) const
    {
        std::shared_lock lock{ m_file_list_mutex };
        if (const FileInfo* p_file_info = file_info(resource_id); p_file_info != nullptr) {
            return p_file_info->entry != nullptr && p_file_info->entry->is_loaded();
        }
        return false;
    }

    /** Unload the least-recently-used resource which is not currently in use.
     * @param unload_all_unused Whether to remove all unused resources instead of just one.
     * @return Whether a resource was unloaded (i.e. there was an unused resource in the cache to
     * unload).
     */
    bool unload_unused(bool unload_all_unused = false) const;

    span<const std::unique_ptr<IFileLoader>> file_loaders() const noexcept
    {
        // No need to lock, since m_file_loaders never changes after construction.
        return m_file_loaders;
    }

    using FileChangeCallbackT = void (*)(const FileChangedEvent&);

    void set_resource_reload_callback(FileChangeCallbackT callback)
    {
        m_resource_reload_callback = std::move(callback);
    }

private:
    FileChangeCallbackT m_resource_reload_callback;

    struct FileInfo {
        Identifier   filename;
        time_point   time_stamp;
        IFileLoader* loader;

        // ResourceEntry associated with this file. Nullptr if not loaded.
        std::unique_ptr<ResourceEntryBase> entry;
    };

    // Rebuilds resource-file-list data structures.
    void rebuild_file_list();

    // Get pointer to FileInfo record for the given filename, or nullptr if no such file exists.
    const FileInfo* file_info(Identifier file) const;
    FileInfo*       file_info(Identifier file)
    {
        return const_cast<FileInfo*>(static_cast<const ResourceCache*>(this)->file_info(file));
    }

    // Get ResourceEntry corresponding to the given FileInfo. If this is the first time the
    // ResourceEntry is requested, then create it.
    template<typename ResT> ResourceEntryBase& get_or_create_resource_entry(FileInfo& file_info)
    {
        // Create ResourcEntry if not present (i.e. this is the first time it is requested).
        if (file_info.entry == nullptr) {
            // Lock to make sure that no other thread is trying to create a ResourceEntry for the
            // same resource at the same time.
            std::unique_lock{ m_set_resource_entry_mutex };

            // Check again after locking, in case another thread did the same thing ahead of us.
            if (file_info.entry == nullptr) {
                file_info.entry = std::make_unique<ResourceEntry<ResT>>(file_info.filename,
                                                                        *file_info.loader,
                                                                        file_info.time_stamp,
                                                                        *this);
            }
        }

        return *file_info.entry;
    }

    // Throw ResourceNotFound exception and write details to log.
    void throw_resource_not_found(Identifier filename) const;

    // Log a message with nice formatting.
    void log_verbose(Identifier resource, std::string_view message) const;
    void log_message(Identifier resource, std::string_view message) const;
    void log_warning(Identifier resource, std::string_view message) const;
    void log_error(Identifier resource, std::string_view message) const;

    // --------------------------------------- Data members ----------------------------------------

    // Loaders for loading resource file data into memory.
    std::vector<std::unique_ptr<IFileLoader>> m_file_loaders;

    // List of resource files available through the resource loaders.
    // Always sorted by filename hash.
    std::vector<FileInfo> m_file_list;

    // Shared mutex to allow multiple readers, single writer.
    mutable std::shared_mutex m_file_list_mutex;

    // Special mutex to allow `get_or_create_resource_entry` to create a new `ResourceEntry` and
    // store it in `FileInfo::entry` without risking race conditions where multiple threads try to
    // create a `ResourceEntry` for the same resource at the same time.
    //
    // Note that this is a single mutex for all FileInfos; however, creating a `ResourceEntry` is a
    // very quick (and rare) operation so this should not matter.
    //
    // Unique-locking `m_file_list_mutex` would be too coarse-grained for this purpose, since any
    // attempt to access a resource would need to do so (any access is, potentially, the first, and
    // so might have to create the ResourceEntry). That would, in particular, cause deadlocks in
    // `ResourceCache::refresh()` when reloading a file that needs to load dependencies:
    // `ResourceCache::refresh()` needs to hold a shared lock on `m_file_list_mutex` as it iterates
    // through the file list; re-loading the resource, in turn, invokes
    // `ResourceCache::resource_handle` as it requests a dependency.
    std::mutex m_set_resource_entry_mutex;
};

} // namespace Mg
