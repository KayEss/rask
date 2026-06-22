#include <rask/data/filetype.hpp>

#include <planet/serialise.hpp>


void rask::data::save(planet::serialise::save_buffer &ab, filetype const ft) {
    ab.save_box(filetype_box, static_cast<std::uint8_t>(ft));
}
void rask::data::load(planet::serialise::box &b, filetype &ft) {
    std::uint8_t value{};
    b.named(filetype_box, value);
    switch (static_cast<filetype>(value)) {
    case filetype::directory:
    case filetype::file:
    case filetype::move_out: ft = static_cast<filetype>(value); return;
    }
    throw felspar::stdexcept::runtime_error{
            "Invalid rask::data::filetype value"};
}
