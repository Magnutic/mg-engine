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

#include "mg/core/mg_resource_cache.h"

#include "mg/core/mg_log.h"
#include "mg/utils/mg_stl_helpers.h"

#include <fmt/core.h>

namespace Mg {

//--------------------------------------------------------------------------------------------------
// ResourceCache implementation
//--------------------------------------------------------------------------------------------------

// Update file index, detects if files have changed (added, removed, changed timestamp).
void ResourceCache::refresh()
{
    rebuild_file_index();

    // Reload & notify for any changed files
    for (FileInfo& file : m_file_list) {
        auto& p_entry = file.entry;

        if (p_entry == nullptr || !p_entry->get_resource().should_reload_on_file_change()) {
            continue;
        }

        // Determine whether to update resource.
        bool should_update = false;

        // First, check whether resource file has been updated.
        if (file.time_stamp > p_entry->time_stamp) { should_update = true; }

        // Then, check whether dependencies have been updated.
        for (auto&& dep : p_entry->dependencies) {
            if (should_update) { break; } // Do not iterate more than necessary.
            if (file_info(dep.dependency_id)->time_stamp > dep.time_stamp) { should_update = true; }
        }

        if (!should_update) { continue; }

        // Reload resource and notify observers.
        log_verbose(file.filename, "Resource was modified, re-loading...");

        try {
            // Create new ResourceEntry, load resource into that, then swap with the old entry.
            auto p_new_entry = p_entry->new_entry(file.filename, file.time_stamp);
            try_load(file, *p_new_entry);
            p_entry->swap_entry(*p_new_entry);
        }
        catch (const ResourceError&) {
            log_error(file.filename, "Resource was modified but re-loading failed.");
            continue;
        }

        m_file_changed_subject.notify(FileChangedEvent{ p_entry->get_resource(), file.time_stamp });
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
void ResourceCache::rebuild_file_index()
{
    log_verbose("<N/A>", "Building file index...");

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
        g_log.write_verbose(fmt::format("Refreshing file index for '{}'", p_loader->name()));

        for (auto&& fr : p_loader->available_files()) { update_file_list(fr, *p_loader); }
    }
}

// Try to load file, unloading unused files if cache is full
void ResourceCache::try_load(const FileInfo& file_info, ResourceEntryBase& entry)
{
    const Identifier filename = file_info.filename;

    LoadResourceParams load_params{ load_resource_data(file_info), *this, entry };
    LoadResourceResult result = entry.get_or_create_resource().load_resource(load_params);

    switch (result.result_code) {
    case LoadResourceResult::Success: entry.last_access = std::chrono::system_clock::now(); return;
    case LoadResourceResult::DataError:
        throw_resource_data_error(filename, result.error_reason);
        break;
    }

    MG_ASSERT(false && "unreachable");
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

// Throw ResourceDataError exception and write details to log.
void ResourceCache::throw_resource_data_error(Identifier filename, std::string_view reason) const
{
    log_error(filename,
              std::string("Failed to load resource, invalid data: " + std::string(reason)));

    throw ResourceDataError{};
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
bool ResourceCache::unload_unused(bool unload_all_unused)
{
    auto is_unused = [](const FileInfo* fi) {
        return fi->entry != nullptr && fi->entry->is_unloadable();
    };

    if (unload_all_unused) {
        log_verbose("<N/A>", "Unloading all unused resources.");
        size_t num_unloaded = 0;

        for (FileInfo& file_info : m_file_list) {
            if (is_unused(&file_info)) {
                file_info.entry->unload();
                ++num_unloaded;
            }
        }

        return num_unloaded > 0;
    }

    // Create vector of FileInfos sorted so that ResourceEntry with earliest last-access time comes
    // first (and thus unload resources in least-recently-used order).
    std::vector<FileInfo*> file_infos;
    for (FileInfo& file_info : m_file_list) { file_infos.push_back(&file_info); }

    sort(file_infos, [&](FileInfo* l, FileInfo* r) {
        if (l->entry == nullptr) { return false; }
        if (r->entry == nullptr) { return true; }
        return l->entry->last_access < r->entry->last_access;
    });

    auto [found, index] = index_where(file_infos, is_unused);

    if (found) {
        ResourceEntryBase& entry_to_unload = *file_infos[index]->entry;
        entry_to_unload.unload();
        log_verbose(file_infos[index]->filename, "Unloaded unused resource.");
    }

    return found;
}

// Load binary data for into memory
std::vector<std::byte> ResourceCache::load_resource_data(const FileInfo& file_info) const
{
    std::vector<std::byte> file_data;
    file_data.resize(file_info.loader->file_size(file_info.filename));

    file_info.loader->load_file(file_info.filename, file_data);
    return file_data;
}

} // namespace Mg
