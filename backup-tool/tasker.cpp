// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// tasker.cpp
//
#include "tasker.hpp"

namespace backup
{

    bool DirectoryCompareTasker::isAllowedToWake() const
    {
        const TaskQueueStatus myStatus{ status() };

        if (!myStatus.isReady() && !myStatus.isDone())
        {
            return false;
        }

        const TaskQueueStatus fileStatus{ m_context.fileCompareTasker().status() };

        const bool tooManyFileTasksWaiting{ (
            fileStatus.queue_size > (fileStatus.resource_count * 2)) };

        return !tooManyFileTasksWaiting;
    }

} // namespace backup
