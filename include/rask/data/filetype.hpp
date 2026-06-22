#pragma once


#include <planet/serialise/forward.hpp>

#include <cstdint>
#include <string_view>


namespace rask::data {


    /// ## Inode type
    /**
     * The kind of inode an entry describes (see `SPEC.md` §4.1, BOOKKEEPING
     * §5). `move_out` is a tombstone marking an inode that was removed or moved
     * away; deletes propagate only through such live events, never through a
     * sweep (see `SPEC.md` §2).
     */
    enum class filetype : std::uint8_t {
        directory,
        file,
        move_out,
    };


    /// ### Serialisation box name
    /**
     * `filetype` is an enumeration and so cannot carry a static `box` member;
     * its box name is this namespace-scope constant instead.
     */
    inline constexpr std::string_view filetype_box = {"r:filetype"};


    /// ### Serialisation
    void save(planet::serialise::save_buffer &, filetype);
    void load(planet::serialise::box &, filetype &);


}
