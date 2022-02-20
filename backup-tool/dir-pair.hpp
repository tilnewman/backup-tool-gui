#ifndef BACKUP_DIR_PAIR_HPP_INCLUDED
#define BACKUP_DIR_PAIR_HPP_INCLUDED
//
// dir-pair.hpp
//
#include "enums.hpp"

#include <cstddef>

namespace backup
{

    template <typename T, typename U = T>
    struct DirPair
    {
        inline auto & get(const WhichDir which) noexcept
        {
            return ((WhichDir::Source == which) ? src : dst);
        }

        inline const auto & get(const WhichDir which) const noexcept
        {
            return ((WhichDir::Source == which) ? src : dst);
        }

        T src;
        U dst;
    };

    using IndexDPair_t = DirPair<std::size_t>;

} // namespace backup

#endif // BACKUP_DIR_PAIR_HPP_INCLUDED
