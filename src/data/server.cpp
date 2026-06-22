#include <rask/data/server.hpp>

#include <planet/serialise.hpp>


void rask::data::save(planet::serialise::save_buffer &ab, server const &s) {
    ab.save_box(server::box, s.identity, s.clock, s.top_hash);
}
void rask::data::load(planet::serialise::box &b, server &s) {
    b.named(server::box, s.identity, s.clock, s.top_hash);
}
