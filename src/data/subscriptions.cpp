#include <rask/data/subscriptions.hpp>

#include <planet/serialise.hpp>


void rask::data::save(planet::serialise::save_buffer &ab, subscription const &s) {
    ab.save_box(subscription::box, s.path);
}
void rask::data::load(planet::serialise::box &b, subscription &s) {
    b.named(subscription::box, s.path);
}


void rask::data::save(
        planet::serialise::save_buffer &ab, subscriptions const &s) {
    ab.save_box(subscriptions::box, s.subscribed);
}
void rask::data::load(planet::serialise::box &b, subscriptions &s) {
    b.named(subscriptions::box, s.subscribed);
}
