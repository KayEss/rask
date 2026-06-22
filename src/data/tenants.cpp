#include <rask/data/tenants.hpp>

#include <planet/serialise.hpp>


void rask::data::save(
        planet::serialise::save_buffer &ab, known_tenant const &kt) {
    ab.save_box(known_tenant::box, kt.tenant_hash);
}
void rask::data::load(planet::serialise::box &b, known_tenant &kt) {
    b.named(known_tenant::box, kt.tenant_hash);
}


void rask::data::save(planet::serialise::save_buffer &ab, tenants const &t) {
    ab.save_box(tenants::box, t.known);
}
void rask::data::load(planet::serialise::box &b, tenants &t) {
    b.named(tenants::box, t.known);
}
