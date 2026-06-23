#include <rask/config/identity.hpp>

#include <planet/serialise.hpp>


void rask::config::save(planet::serialise::save_buffer &ab, node_id const &n) {
    ab.save_box(node_id::box, n.value);
}
void rask::config::load(planet::serialise::box &b, node_id &n) {
    b.named(node_id::box, n.value);
}
