#include <rask/wire/tick.hpp>

#include <planet/serialise.hpp>


void rask::wire::save(planet::serialise::save_buffer &ab, tick const &t) {
    ab.save_box(tick::box, t.time, t.server, t.reserved);
}
void rask::wire::load(planet::serialise::box &b, tick &t) {
    b.named(tick::box, t.time, t.server, t.reserved);
}
