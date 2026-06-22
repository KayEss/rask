#include <rask/data/inode.hpp>

#include <planet/serialise.hpp>


void rask::data::save(planet::serialise::save_buffer &ab, file_stat const &s) {
    ab.save_box(file_stat::box, s.size, s.modified);
}
void rask::data::load(planet::serialise::box &b, file_stat &s) {
    b.named(file_stat::box, s.size, s.modified);
}


void rask::data::save(planet::serialise::save_buffer &ab, remote_state const &r) {
    ab.save_box(remote_state::box, r.priority, r.size, r.content);
}
void rask::data::load(planet::serialise::box &b, remote_state &r) {
    b.named(remote_state::box, r.priority, r.size, r.content);
}


void rask::data::save(planet::serialise::save_buffer &ab, inode const &i) {
    ab.save_box(
            inode::box, i.type, i.name, i.content, i.priority, i.stat,
            i.remote);
}
void rask::data::load(planet::serialise::box &b, inode &i) {
    b.named(inode::box, i.type, i.name, i.content, i.priority, i.stat,
            i.remote);
}


void rask::data::save(planet::serialise::save_buffer &ab, inode_tree const &t) {
    ab.save_box(inode_tree::box, t.tenant, t.inodes);
}
void rask::data::load(planet::serialise::box &b, inode_tree &t) {
    b.named(inode_tree::box, t.tenant, t.inodes);
}
