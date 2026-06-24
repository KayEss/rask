#include <felspar/io/posix.hpp>
#include <planet/log.hpp>


int main() {
    auto const [old_limit, new_limit] =
            felspar::posix::promise_to_never_use_select();
    planet::log::info(
            "rask started -- raised open file descriptor limit from", old_limit,
            "to", new_limit);
    return 0;
}
