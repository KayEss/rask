#include <rask/data/peers.hpp>

#include <planet/serialise.hpp>


void rask::data::save(planet::serialise::save_buffer &ab, peer const &p) {
    ab.save_box(peer::box, p.host, p.port);
}
void rask::data::load(planet::serialise::box &b, peer &p) {
    b.named(peer::box, p.host, p.port);
}


void rask::data::save(planet::serialise::save_buffer &ab, peers const &p) {
    ab.save_box(peers::box, p.connect);
}
void rask::data::load(planet::serialise::box &b, peers &p) {
    b.named(peers::box, p.connect);
}
