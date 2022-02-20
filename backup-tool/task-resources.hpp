#ifndef BACKUP_TASK_RESOURCES_HPP_INCLUDED
#define BACKUP_TASK_RESOURCES_HPP_INCLUDED
//
// task-resources.hpp
//
#include "dir-pair.hpp"
#include "entry.hpp"
#include "enums.hpp"
#include "filesystem-common.hpp"
#include "util.hpp"

#include <cassert>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace backup
{

    constexpr std::size_t reserveCount{ 4096 };

    //
    using EntryVec_t           = std::vector<Entry>;
    using EntryDPair_t         = DirPair<Entry>;
    using EntryConstRefDPair_t = DirPair<const Entry &, const Entry &>;

    //

    // since one of the TaskResource classes needs to use this clock to keep track of time elapsed,
    // we are forced to use whatever that clock uses for a progress counter type
    using Progress_t = Clock_t::rep;

    //

    template <typename Resource_t>
    struct ScopedResource;

    struct TaskResourcesBase
    {
        virtual ~TaskResourcesBase() = default;

        virtual void setup() { progress = 0; }

        // must always be safe to call this function at any time, repeatedly, from the owning thread
        virtual void teardown() {}

        EntryDPair_t entry_dpair;

        // different derived TaskResource classes have different meanings for this counter
        Progress_t progress = 0;

        inline bool isAvailable() const noexcept { return is_available; }

        // this is private with ScopedResource a friend to ensure only ScopedResource changes it
      private:
        template <typename Resource_t>
        friend struct ScopedTaskResource;

        bool is_available = true;
    };

    //

    // progress is the total bytes copied so far
    using CopyTaskResources = TaskResourcesBase;

    //

    // progress is the nanoseconds since the epoch at the time the delete started,
    // allowing trackoing of how long each delete operations has been executing
    struct RemoveTaskResources : public TaskResourcesBase
    {
        virtual ~RemoveTaskResources() = default;

        void setup() override
        {
            TaskResourcesBase::setup();
            progress = Clock_t::now().time_since_epoch().count();
        }
    };

    //

    struct FileReadResources
    {
        FileReadResources()
            : buffer(max_read_size)
            , stream()
        {}

        ~FileReadResources()
        {
            try
            {
                close();
            }
            catch (...)
            {
            }
        }

        void open(const fs::path & path)
        {
            if (stream.is_open())
            {
                close();
            }

            stream.clear();
            stream.open(path, (std::ios::binary | std::ios::in));
        }

        void close()
        {
            if (stream.is_open())
            {
                stream.close();
                stream.clear();
            }
        }

        std::vector<char> buffer;
        InputFileStream_t stream;
        static inline constexpr std::size_t min_read_size{ 1 << 14 };
        static inline constexpr std::size_t max_read_size{ 1 << 20 };
    };

    //

    // progress is the current progress percent (0-100)
    struct FileCompareTaskResources : public TaskResourcesBase
    {
        virtual ~FileCompareTaskResources() = default;

        void setup() override
        {
            TaskResourcesBase::setup();
            file_dpair.src.open(entry_dpair.src.path);
            file_dpair.dst.open(entry_dpair.dst.path);
        }

        void teardown() override
        {
            TaskResourcesBase::teardown();
            file_dpair.src.close();
            file_dpair.dst.close();
        }

        DirPair<FileReadResources, FileReadResources> file_dpair;
    };

    //

    // progress is not used
    struct DirectoryCompareTaskResources : public TaskResourcesBase
    {
        virtual ~DirectoryCompareTaskResources() = default;

        DirectoryCompareTaskResources()
            : file_entrys_dpair()
            , dir_entrys_dpair()
        {
            file_entrys_dpair.src.reserve(reserveCount);
            file_entrys_dpair.dst.reserve(reserveCount);
            dir_entrys_dpair.src.reserve(reserveCount);
            dir_entrys_dpair.dst.reserve(reserveCount);
        }

        void setup() override
        {
            TaskResourcesBase::setup();
            clearAll();
        }

        void teardown() override
        {
            TaskResourcesBase::teardown();
            clearAll();
        }

        void clearAll()
        {
            file_entrys_dpair.src.clear();
            file_entrys_dpair.dst.clear();
            dir_entrys_dpair.src.clear();
            dir_entrys_dpair.dst.clear();
        }

        DirPair<EntryVec_t> file_entrys_dpair;
        DirPair<EntryVec_t> dir_entrys_dpair;
    };

    // This is a helper class used by ResourceLimitedParallelTaskQueue (see below) that uses RAII to
    // ensure that setup()/teardown()/is_available are always correctly called/set.
    template <typename Resource_t>
    struct ScopedTaskResource
    {
        //  creates an invalid object that never touches the resource or the mutex
        ScopedTaskResource(const bool isValid, std::mutex & mut, Resource_t & res)
            : is_valid(isValid)
            , resource(res)
            , mutex(mut)
        {
            if (is_valid)
            {
                assert(resource.is_available);
                resource.is_available = false;
            }
        }

        ~ScopedTaskResource()
        {
            if (is_valid)
            {
                std::scoped_lock scopedLock(mutex);
                assert(!resource.is_available);
                resource.is_available = true;
            }
        }

        // move only
        ScopedTaskResource(ScopedTaskResource &&) noexcept = default;
        ScopedTaskResource & operator=(ScopedTaskResource &&) noexcept = default;
        ScopedTaskResource(const ScopedTaskResource &)                 = delete;
        ScopedTaskResource & operator=(const ScopedTaskResource &) = delete;

        const bool is_valid;
        Resource_t & resource;

      private:
        std::mutex & mutex;
    };

} // namespace backup

#endif // BACKUP_TASK_RESOURCES_HPP_INCLUDED
