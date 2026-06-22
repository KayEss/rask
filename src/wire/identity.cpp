#include <rask/wire/identity.hpp>

#include <planet/serialise.hpp>


void rask::wire::save(planet::serialise::save_buffer &ab, node_id const &n) {
    ab.save_box(node_id::box, n.value);
}
void rask::wire::load(planet::serialise::box &b, node_id &n) {
    b.named(node_id::box, n.value);
}
