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

// TODO: consider thread safety, currently not thread-safe.

#pragma once

#include "mg/core/mg_file_loader.h"
#include "mg/core/mg_identifier.h"
#include "mg/core/mg_resource_entry_base.h"
#include "mg/core/mg_resource_exceptions.h"
#include "mg/core/mg_resource_handle.h"
#include "mg/resources/mg_base_resource.h"
#include "mg/resources/mg_file_changed_event.h"
#include "mg/utils/mg_macros.h"
#include "mg/utils/mg_observer.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Mg {

class ResourceCache;

/** Input to resource types' `load_resource()` member function. */
class LoadResourceParams {
public:
    span<const std::byte> resource_data() const noexcept { return m_data; }

    std::string_view resource_data_as_text() const noexcept
    {
        return { reinterpret_cast<const char*>(m_data.data()), m_data.size() }; // NOLINT
    }

    /** Load a resource and mark this resource as dependent on the newly loaded resource. */
    template<typename ResT>
    ResourceAccessGuard<ResT> load_dependency(Identifier dependency_file_id) const;

private:
    friend class ResourceCache;

    explicit LoadResourceParams(std::vector<std::byte> data,
                                ResourceCache&         owning_cache,
                                ResourceEntryBase&     resource_entry)
        : m_data(std::move(data)), m_owning_cache(&owning_cache), m_resource_entry(&resource_entry)
    {}

    std::vector<std::byte> m_data;
    ResourceCache*         m_owning_cache;
    ResourceEntryBase*     m_resource_entry;
};

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
 * The cache maintains an index of files available to its resource loaders. This allows the cache to
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
    template<typename... LoaderTs>
    explicit ResourceCache(size_t temp_compat_dummy, std::unique_ptr<LoaderTs>... file_loaders)

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

    /** Update file index, detects if files have changed (added, removed, changed timestamp).
     * Thread safety: should not be called when other threads are accessing resources.
     */
    void refresh();

    /** Get resource from file (or cache).
     * @param file Filename (path) to resource file.
     */
    template<typename ResT> ResourceAccessGuard<ResT> access_resource(Identifier file);

    /** Get handle to a resource with the given path.
     * @param file Filename (path) to resource file.
     * @load_resource_immediately Whether to load the resource into the cache before returning the
     * handle, or to defer loading until first access.
     */
    template<typename ResT>
    ResourceHandle<ResT> resource_handle(Identifier file, bool load_resource_immediately = true);

    /** Returns whether a file with the given path exists in the file index.
     * N.B. returns the state as of most recent call to `refresh()`
     */
    bool file_exists(Identifier file) const { return file_info(file) != nullptr; }

    /** Returns the time stamp of the given file. Throws if file does not exist in file index. */
    time_point file_time_stamp(Identifier file) const
    {
        auto p_file_info = file_info(file);
        if (!p_file_info) { throw_resource_not_found(file); }
        return p_file_info->time_stamp;
    }

    /** Returns whether the resource with given id is currently cached in this ResourceCache. */
    bool is_cached(Identifier resource_id) const
    {
        if (const FileInfo* p_file_info = file_info(resource_id); p_file_info != nullptr) {
            return p_file_info->entry && p_file_info->entry->is_loaded();
        }
        return false;
    }

    /** Unload the least-recently-used resource which is not currently in use.
     * @param unload_all_unused Whether to remove all unused resources instead of just one.
     * @return Whether a resource was unloaded (i.e. there was an unused resource in the cache to
     * unload).
     */
    bool unload_unused(bool unload_all_unused = false);

    span<const std::unique_ptr<IFileLoader>> file_loaders() const noexcept
    {
        return m_file_loaders;
    }

    /** Add Observer to be notified whenever a resource has been re-loaded as a result of its
     * file changing.
     */
    void add_file_changed_observer(Observer<FileChangedEvent>& observer)
    {
        m_file_changed_subject.add_observer(observer);
    }

    void load_into_resource_entry(ResourceEntryBase& entry)
    {
        FileInfo* p_file_info = file_info(entry.resource_id);
        MG_ASSERT(p_file_info != nullptr);
        MG_ASSERT(p_file_info->entry.get() == &entry);

        try_load(*p_file_info, entry);
    }

private:
    /** Subject notifying Observers whenever a resource has been re-loaded as a result of its file
     * changing.
     */
    Subject<FileChangedEvent> m_file_changed_subject;

    struct FileInfo {
        Identifier   filename;
        time_point   time_stamp;
        IFileLoader* loader;

        // ResourceEntry associated with this file. Nullptr if not loaded.
        std::unique_ptr<ResourceEntryBase> entry;
    };

    // Rebuilds resource file index data structures.
    void rebuild_file_index();

    // Get pointer to FileInfo record for the given filename, or nullptr if no such file exists.
    const FileInfo* file_info(Identifier file) const;
    FileInfo*       file_info(Identifier file)
    {
        return const_cast<FileInfo*>(static_cast<const ResourceCache*>(this)->file_info(file));
    }

    // Load binary data for into memory
    std::vector<std::byte> load_resource_data(const FileInfo& file_info) const;

    // Try to load file, unloading unused files if cache is full
    void try_load(const FileInfo& file_info, ResourceEntryBase& entry);

    template<typename ResT> ResourceEntryBase& get_or_create_resource_entry(FileInfo& file_info)
    {
        static_assert(std::is_base_of_v<BaseResource, ResT>,
                      "Type must be derived from Mg::BaseResource.");
        static_assert(!std::is_abstract_v<ResT>, "Resource types must not be abstract.");
        static_assert(std::is_constructible_v<ResT, Identifier>);

        if (!file_info.entry) {
            file_info.entry = std::make_unique<ResourceEntry<ResT>>(
                file_info.filename, file_info.time_stamp, *this);
        }

        return *file_info.entry;
    }

    // Throw ResourceNotFound exception and write details to log.
    void throw_resource_not_found(Identifier filename) const;

    // Throw ResourceDataError exception and write details to log.
    void throw_resource_data_error(Identifier filename, std::string_view reason) const;

    // Throw ResourceCacheOutOfMemory exception and write details to log.
    void throw_resource_cache_oom(Identifier filename) const;

    // Log a message with nice formatting.
    void log_verbose(Identifier resource, std::string_view message) const;
    void log_message(Identifier resource, std::string_view message) const;
    void log_warning(Identifier resource, std::string_view message) const;
    void log_error(Identifier resource, std::string_view message) const;

    // --------------------------------------- Data members ----------------------------------------

    // Loaders for loading resources
    std::vector<std::unique_ptr<IFileLoader>> m_file_loaders;

    // List of resource files available through the resource loaders.
    // Always sorted by filename hash.
    std::vector<FileInfo> m_file_list;
};

//--------------------------------------------------------------------------------------------------
// ResourceCache member function template implementations
//--------------------------------------------------------------------------------------------------

/** Get resource from file (or cache). */
template<typename ResT>
ResourceAccessGuard<ResT> ResourceCache::access_resource(Identifier filename)
{
    log_verbose(filename, "Accessing file.");

    // Check for file in known-files list.
    FileInfo* p_file_info = file_info(filename);
    if (p_file_info == nullptr) { throw_resource_not_found(filename); }

    ResourceEntryBase& entry = get_or_create_resource_entry<ResT>(*p_file_info);
    return static_cast<ResourceEntry<ResT>&>(entry).access_resource();
}

template<typename ResT>
ResourceHandle<ResT> ResourceCache::resource_handle(Identifier file, bool load_resource_immediately)
{
    FileInfo* p_file_info = file_info(file);
    if (!p_file_info) { throw_resource_not_found(file); }

    ResourceEntryBase&   entry = get_or_create_resource_entry<ResT>(*p_file_info);
    ResourceHandle<ResT> handle(static_cast<ResourceEntry<ResT>&>(entry));

    if (load_resource_immediately) handle.access();

    return handle;
}

//--------------------------------------------------------------------------------------------------
// ResourceDataLoader member function implementations
//--------------------------------------------------------------------------------------------------

template<typename ResT>
ResourceAccessGuard<ResT> LoadResourceParams::load_dependency(Identifier dependency_file_id) const
{
    time_point file_time_stamp = m_owning_cache->file_time_stamp(dependency_file_id);
    m_resource_entry->dependencies.push_back({ dependency_file_id, file_time_stamp });
    return m_owning_cache->access_resource<ResT>(dependency_file_id);
}

} // namespace Mg
