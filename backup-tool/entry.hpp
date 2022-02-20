#ifndef BACKUP_ENTRY_HPP_INCLUDED
#define BACKUP_ENTRY_HPP_INCLUDED
//
// entry.hpp
//
#include "dir-pair.hpp"
#include "enums.hpp"
#include "filesystem-common.hpp"

#include <string>

namespace backup
{

    struct Entry
    {
        Entry() = default;

        Entry(
            const WhichDir dirParam,
            const bool isFileParam,
            const fs::path & pathParam,
            const std::size_t sizeParam)
            : which_dir(dirParam)
            , is_file(isFileParam)
            , path(pathParam.filename()) // use the path member as a temporary
            , name(path.wstring())
            , extension(path.extension().wstring())
            , size(sizeParam)
        {
            if (path.empty())
            {
                name = pathParam.root_name().wstring();
                name += pathParam.root_directory().wstring();
                extension.clear();
            }

            path = pathParam;
        }

        Entry(const Entry &) = default;
        Entry & operator=(const Entry &) = default;

        // enable noexcept moves because there will be many std::vetors of these...
        Entry(Entry &&) noexcept = default;
        Entry & operator=(Entry &&) noexcept = default;

        inline bool isEmpty() const noexcept { return path.empty(); }

        inline void makeEmpty() { path.clear(); }

        WhichDir which_dir = WhichDir::Source;
        bool is_file       = false;
        fs::path path;
        std::wstring name;
        std::wstring extension;
        std::size_t size = 0;
    };

} // namespace backup

#endif // BACKUP_ENTRY_HPP_INCLUDED
