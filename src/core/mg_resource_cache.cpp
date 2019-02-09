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

#include <stdexcept>

#include "mg/utils/mg_stl_helpers.h"

namespace Mg {

//--------------------------------------------------------------------------------------------------
// ResourceDataLoader implementation
//--------------------------------------------------------------------------------------------------

size_t ResourceDataLoader::file_size() const
{
    return m_file_loader->file_size(resource_id());
}

void ResourceDataLoader::load_file(span<std::byte> output_buffer) const
{
    m_file_loader->load_file(resource_id(), output_buffer);
}

memory::DefragmentingAllocator& ResourceDataLoader::allocator() const noexcept
{
    return m_owning_cache->allocator();
}

Identifier ResourceDataLoader::resource_id() const noexcept
{
    return m_resource_entry->get_resource().resource_id();
}

//--------------------------------------------------------------------------------------------------
// ResourceCache implementation
//--------------------------------------------------------------------------------------------------

// Update file index, detects if files have changed (added, removed, changed timestamp).
void ResourceCache::refresh()
{
    rebuild_file_index();

    // Reload & notify for any changed files
    for (ResEntryOwningPtr& p_entry : m_resources) {
        if (!p_entry->get_resource().should_reload_on_file_change()) { continue; }

        Identifier      filename    = p_entry->get_resource().resource_id();
        const FileInfo* p_file_info = file_info(filename);

        // If file does not exist in the data source (any more), move on to next resource.
        if (p_file_info == nullptr) { continue; }

        auto new_time_stamp = p_file_info->time_stamp;

        // Determine whether to update resource.
        bool should_update = false;

        // First, check whether resource file has been updated.
        if (new_time_stamp > p_entry->time_stamp) { should_update = true; }

        // Then, check whether dependencies have been updated.
        for (auto&& dep : p_entry->dependencies) {
            if (should_update) {
                break; // Do not iterate more than necessary.
            }

            if (file_info(dep.dependency_id)->time_stamp > dep.time_stamp) { should_update = true; }
        }

        if (!should_update) { continue; }

        // Reload resource and notify observers.
        g_log.write_message(
            fmt::format("Resource '{}' was modified, loading...", filename.c_str()));

        try {
            ResourceDataLoader loader{ *this, *p_file_info->loader, *p_entry };
            p_entry->dependencies.clear();
            p_entry->get_resource().load_resource(loader);
        }
        catch (std::exception& e) {
            g_log.write_error(
                fmt::format("Failed to load resource '{}':\n\t'{}'", filename.c_str(), e.what()));
            continue;
        }

        m_file_changed_subject.notify(FileChangedEvent{ p_entry->get_resource(), new_time_stamp });
    }
}

const ResourceCache::FileInfo* ResourceCache::file_info(Identifier file) const
{
    auto [found, index] = index_where(m_file_list, [&](auto&& af) { return af.filename == file; });
    if (!found) { return nullptr; }
    return &m_file_list[index];
}

// Rebuilds available-file list data structure.
void ResourceCache::rebuild_file_index()
{
    g_log.write_verbose(
        fmt::format("ResourceCache[{}]: building file index...", static_cast<void*>(this)));

    for (auto&& p_loader : file_loaders()) {
        MG_ASSERT(p_loader != nullptr);
        g_log.write_verbose(fmt::format("Refreshing file index for '{}'", p_loader->name()));
        std::vector<FileRecord> files = p_loader->available_files();

        for (auto&& fr : files) {
            m_file_list.push_back({ fr.name, fr.time_stamp, p_loader.get() });
        }
    }

    auto order_by_filename_hash_and_greater_time_stamp = [](auto&& af1, auto&& af2) {
        auto hash1 = af1.filename.hash();
        auto hash2 = af2.filename.hash();
        return (hash1 < hash2) || (hash1 == hash2 && af1.time_stamp > af2.time_stamp);
    };

    auto same_filename = [](auto&& af1, auto&& af2) { return af1.filename == af2.filename; };

    // Filter duplicate files, keeping the one with greater timestamp.
    sort(m_file_list, order_by_filename_hash_and_greater_time_stamp);
    m_file_list.erase(std::unique(m_file_list.begin(), m_file_list.end(), same_filename));
}

std::runtime_error ResourceCache::make_not_found_exception(Identifier filename)
{
    std::string msg = fmt::format(
        "ResourceCache[{}]: No such file '{}'.", static_cast<void*>(this), filename.c_str());

    msg += " [ searched in ";
    for (auto&& p_loader : file_loaders()) { msg += fmt::format("'{}' ", p_loader->name()); }
    msg += ']';

    return std::runtime_error(msg);
}

// Get iterator to entry corresponding to the given Identifier if the entry is in cache.
auto ResourceCache::get_if_loaded(Identifier file) const -> ResEntryPtr
{
    auto [found, index] =
        index_where(m_resources, [&](auto&& e) { return e->get_resource().resource_id() == file; });

    if (!found) { return nullptr; }
    return m_resources[index];
}

// Unload the least recently used resource for which is not currently in use.
bool ResourceCache::unload_unused(bool unload_all_unused)
{
    auto is_unused = [](auto&& e) { return e->ref_count == 0; };

    if (unload_all_unused) {
        g_log.write_verbose(fmt::format("ResourceCache[{}]: unloading all unused resources.",
                                        static_cast<void*>(this)));
        return find_and_erase_if(m_resources, is_unused);
    }

    // Sort so that ResourceEntry with earliest last-access time comes first.
    // The ordering of m_resources does not matter outside of this function; by sorting it, it will
    // likely be almost sorted in the next call of this function.
    sort(m_resources, [&](auto& e1, auto& e2) { return e1->last_access < e2->last_access; });

    auto [found, index] = index_where(m_resources, is_unused);

    if (found) {
        auto resource_id = m_resources[index]->get_resource().resource_id();
        g_log.write_verbose(fmt::format(
            "ResourceCache[{}]: unloading '{}'.", static_cast<void*>(this), resource_id.c_str()));
        m_resources.erase(m_resources.begin() + ptrdiff_t(index));
    }

    return found;
}

} // namespace Mg
