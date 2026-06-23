#include <rask/config/peers.hpp>

#include <planet/serialise.hpp>


void rask::config::save(planet::serialise::save_buffer &ab, peer const &p) {
    ab.save_box(peer::box, p.host, p.port);
}
void rask::config::load(planet::serialise::box &b, peer &p) {
    b.named(peer::box, p.host, p.port);
}


void rask::config::save(planet::serialise::save_buffer &ab, peers const &p) {
    ab.save_box(peers::box, p.connect);
}
void rask::config::load(planet::serialise::box &b, peers &p) {
    b.named(peers::box, p.connect);
}
