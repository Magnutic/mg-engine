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

#include "mg/resource_cache/mg_resource_cache.h"

#include "mg/core/mg_log.h"
#include "mg/resource_cache/mg_base_resource.h"
#include "mg/resource_cache/mg_resource_exceptions.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

namespace Mg {

namespace {
std::string format_time(std::time_t time)
{
    const auto time_gm = std::gmtime(&time);
    if (!time_gm) { return "<INVALID TIME>"; }

    constexpr size_t maxlen = 100;
    std::string      time_string(maxlen, ' ');
    const auto       len = std::strftime(time_string.data(), maxlen, "%c %Z", time_gm);
    time_string.resize(len);
    return time_string;
}
} // namespace

//--------------------------------------------------------------------------------------------------
// ResourceCache implementation
//--------------------------------------------------------------------------------------------------

// Update file list, detects if files have changed (added, removed, changed timestamp).
void ResourceCache::refresh()
{
    // Refresh list of available files.
    {
        std::unique_lock lock{ m_file_list_mutex };
        rebuild_file_list();
    }

    auto has_file_changed = [this](const ResourceCache::FileInfo& file,
                                   std::time_t                    old_time_stamp) -> bool {
        bool has_changed = file.time_stamp > old_time_stamp;

        if (has_changed) {
            g_log.write_message(fmt::format(
                "Detected that {} has changed (old time-stamp: {}, new time-stamp: {}).",
                file.filename.str_view(),
                format_time(old_time_stamp),
                format_time(file.time_stamp)));
        }

        return has_changed;
    };

    // Has any of the given ResourceEntry's dependencies' files changed since they were loaded?
    auto dependencies_were_updated = [&](const ResourceEntryBase& entry) -> bool {
        auto dependency_was_updated = [&](const ResourceEntryBase::Dependency& dependency) {
            return has_file_changed(*file_info(dependency.dependency_id), dependency.time_stamp);
        };

        return find_if(entry.dependencies, dependency_was_updated) != entry.dependencies.end();
    };

    // Should the resource corresponding to the given file be re-loaded (i.e. has its file or those
    // of any of its dependencies changed)?
    auto should_reload = [&](const FileInfo& fi) -> bool {
        if (fi.entry == nullptr) { return false; }

        std::shared_lock entry_lock{ fi.entry->mutex };

        if (!fi.entry->is_loaded() || !fi.entry->get_resource().should_reload_on_file_change()) {
            return false;
        }

        // First, check whether resource file has been updated.
        if (has_file_changed(fi, fi.entry->time_stamp())) { return true; }

        // Then, check whether dependencies have been updated.
        return dependencies_were_updated(*fi.entry);
    };

    struct ReloadInfo {
        ResourceEntryBase& entry;
        Identifier         resource_type_id;
        std::time_t        new_time_stamp;
        IFileLoader&       new_loader;
    };
    std::vector<ReloadInfo> entries_to_reload;

    // Scope for lock.
    {
        std::shared_lock lock{ m_file_list_mutex };

        // Create list of files to reload and unload the resources.
        for (const FileInfo& file : m_file_list) {
            if (should_reload(file)) {
                std::unique_lock entry_lock(file.entry->mutex);
                entries_to_reload.push_back(ReloadInfo{
                    *file.entry, file.entry->resource_type_id(), file.time_stamp, *file.loader });
                file.entry->unload();
            }
        }
    }

    // Notify callbacks of file changes.
    for (const auto& [entry, resource_type, new_time_stamp, new_loader] : entries_to_reload) {
        if (m_resource_reload_callback) {
            BaseResourceHandle handle(entry.resource_type_id(), entry);
            m_resource_reload_callback(FileChangedEvent{ handle, resource_type, new_time_stamp });
        }
    }
}

// Compare FileInfo struct and Identifier by filename hash.
// Used for binary search / ordered insertion in m_file_list.
static auto cmp_filename = [](const auto& file_info, Identifier r) {
    return file_info.filename.hash() < r.hash();
};

const ResourceCache::FileInfo* ResourceCache::file_info(Identifier file) const
{
    // m_file_list is sorted by filename hash, so we can look up with binary search.
    auto it = std::lower_bound(m_file_list.begin(), m_file_list.end(), file, cmp_filename);
    if (it == m_file_list.end() || it->filename != file) { return nullptr; }
    return std::addressof(*it);
}

// Rebuilds available-file list data structure.
void ResourceCache::rebuild_file_list()
{
    // Caller (i.e. refresh()) is responsible for locking m_file_list_mutex.
    log_verbose("<N/A>", "(Re-) Building file list...");

    // Update file list with the new file record.
    auto update_file_list = [&](const FileRecord& file_record, IFileLoader& loader) {
        const auto filename = file_record.name;
        auto it = std::lower_bound(m_file_list.begin(), m_file_list.end(), filename, cmp_filename);

        // If not found, insert new FileInfo
        if (it == m_file_list.end() || it->filename != filename) {
            m_file_list.insert(it, FileInfo{ filename, file_record.time_stamp, &loader, nullptr });
            return;
        }

        // Update old FileInfo, if new record has greater time stamp.
        if (file_record.time_stamp > it->time_stamp) {
            it->time_stamp = file_record.time_stamp;
            it->loader     = &loader;
        }
    };

    for (auto&& p_loader : file_loaders()) {
        MG_ASSERT(p_loader != nullptr);
        g_log.write_verbose(fmt::format("Refreshing file list for '{}'", p_loader->name()));

        for (auto&& fr : p_loader->available_files()) { update_file_list(fr, *p_loader); }
    }
}

// Throw ResourceNotFound exception and write details to log.
void ResourceCache::throw_resource_not_found(Identifier filename) const
{
    std::string msg = "No such file. [ searched in ";
    for (auto&& p_loader : file_loaders()) { msg += fmt::format("'{}' ", p_loader->name()); }
    msg += ']';

    log_error(filename, msg);
    throw ResourceNotFound{};
}

// Log a message with nice formatting.
static void
log(Log::Prio prio, const ResourceCache* origin, Identifier resource, std::string_view message)
{
    std::string msg = fmt::format("ResourceCache[{}]: {} [resource: {}]",
                                  static_cast<const void*>(origin),
                                  message,
                                  resource.c_str());
    g_log.write(prio, msg);
}

void ResourceCache::log_verbose(Identifier resource, std::string_view message) const
{
    log(Log::Prio::Verbose, this, resource, message);
}
void ResourceCache::log_message(Identifier resource, std::string_view message) const
{
    log(Log::Prio::Message, this, resource, message);
}
void ResourceCache::log_warning(Identifier resource, std::string_view message) const
{
    log(Log::Prio::Warning, this, resource, message);
}
void ResourceCache::log_error(Identifier resource, std::string_view message) const
{
    log(Log::Prio::Error, this, resource, message);
}

// Unload the least recently used resource for which is not currently in use.
bool ResourceCache::unload_unused(bool unload_all_unused) const
{
    // unload_unused does not modify anything in the file list itself, so shared lock suffices.
    std::shared_lock lock{ m_file_list_mutex };

    auto is_unloadable = [](const ResourceEntryBase& entry) -> bool {
        return entry.ref_count == 0 && entry.is_loaded();
    };

    auto try_unload = [&](const FileInfo& file_info) -> bool {
        using namespace std::chrono_literals;
        if (file_info.entry == nullptr || file_info.entry->ref_count != 0) { return false; }

        ResourceEntryBase& entry = *file_info.entry;
        std::unique_lock   entry_lock{ entry.mutex, 100ms };

        if (!entry_lock.owns_lock() || !is_unloadable(entry)) { return false; }

        entry.unload();
        log_verbose(file_info.filename, "Unloaded unused resource.");
        return true;
    };

    if (unload_all_unused) {
        log_verbose("<N/A>", "Unloading all unused resources.");
        size_t num_unloaded = 0;

        for (const FileInfo& file_info : m_file_list) {
            if (try_unload(file_info)) { ++num_unloaded; }
        }

        return num_unloaded > 0;
    }

    // Create vector of FileInfos sorted so that ResourceEntry with earliest last-access time
    // comes first (and thus unload resources in least-recently-used order).
    std::vector<const FileInfo*> file_infos;
    for (const FileInfo& file_info : m_file_list) { file_infos.push_back(&file_info); }

    auto lru_order = [](const FileInfo* l, const FileInfo* r) {
        if (l->entry == nullptr) { return false; }
        if (r->entry == nullptr) { return true; }
        return l->entry->last_access < r->entry->last_access;
    };

    sort(file_infos, lru_order);

    for (const FileInfo* p_file_info : file_infos) {
        if (try_unload(*p_file_info)) { return true; }
    }

    return false;
}

} // namespace Mg
