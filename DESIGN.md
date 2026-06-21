# Meta-data #

We need to store meta-data about files and directories in a way that all nodes can agree on and we want to be able to quickly determine if there are files that need to be updated.

## The data tree ##

The data is managed through an n-bit tree. The nodes are then numbered in a way that gives a limit to the amount of data. There are three levels:

1. The top-level directories that are managed, called tentants
2. The file system nodes that are managed (directories and files)
3. The file data

The tree is slightly different for files compared to tenants and file system nodes.

For files it's relatively simple, each file block gets a hash. These are the leaf hashes. Leaves are aggregated into blocks of 1024 each (a 32KB block holds 1024 of the 32 byte hashes) which then gives a new hash at the next level. We keep adding leaves and for every 1024 hashes we add a new hash at the next level out. When this outer level contains 1024 hashes we then open another layer which gets a hash of these hashes, and so on as the leaf hash list grows.

When we've finished hashing the file if the outermost group of hashes contains more than a single hash then these are hashed to give an outermost level with a single hash. This is the hash for the file. Note that this means that for a file of up to a single block in size this hash is always the file hash.

The tree for tenants and file nodes need to deal with names rather than file data. A leaf node contains the names of interest. In order to stop a leaf node from becoming too large it will be split when it crosses the split size threshold. A parent node is created which then contains a number of child nodes. Names are assigned to children based on the hash of their name. This ensures a good distribution of names between hashes. When names are removed they are removed from the leaf nodes as expected. If enough names are removed and the total number of names across all leaves in the parent node falls below the coalescing threshold then the names are brought into a single new leaf node which replaces the parent node.

The split and coalesce thresholds need to be wide enough apart that it doesn't thrash as files are added and removed.

For MVP all hashes will be stored in local beanbags. It's expected that these should be on a separate spinning disk to the tenants that are being managed.

## Hashes ##

For each file of interest we are going to generate a hash. We want a fast hashing algorithm with reasonable collision avoidance. The hash will be used to determine if the file contents are correct or not. 256 bit SHA2 should be fine for this purpose.

There are three levels that we care about, corresponding to the three levels of the data tree.

When storing the hashes we will use base 32 so that each character corresponds to one level in the 5 bit tree.

### File hashes ###

The file is broken into data blocks of a fixed size (to be determined, but probably around 32KB). These are then grouped into blocks of 1024. These are also grouped into 32KB chunks until we get a layer (which could be the raw file data) that is below 32KB so it's entire content can be expressed by a single hash. This final hash becomes the file hash that is embedded in the inode structure.


## Sweeps ##

When a new top level directory is published we must perform a sweep to ensure that we have the correct meta-data about all of the files. We also preform a sweep when the rask server loads.


# Protocol #

We need the protocol to be low overhead and fast. After the initial connection the protocol is totally symmetric -- that is, both sides may initiate a send of data and both sides may make a determination of what to send the other based on the data that they have about the connection.

The protocol is stateful -- that is, each node maintains a view of the state at each other node and sends the data that it judges to be wanted by the other nodes. If data that a node wants isn't actually

All numbers are sent in network byte order, that is big endian.


## Initial handshake ##

After the initial connection each host will always send a version block. The minimum common version number is the one that is to be used as that is the highest version number both hosts understand.


## Event packets ##

The minimal set of commands that are needed in order to be able to replicate are:

* a hash value block for part of the data tree
* a data block containing file data

Additional commands allow optimisation. The two nodes that are communicating are trying to achieve the same top level hash. When the top level hashes no longer coincide then they will exchange hashes of parts of the tree below in order to find parts of the file trees that differ between them so that they can exchange the file data needed to bring them back into sync.

The data packet header consists of a single byte (actually a variable length byte as outlined below) which is then followed by a variable length byte describing the size of the command content.

* 0x80 -- version block
* 0x81 -- tenant details
* 0x82 -- hash values for files/directories in a tenant
* 0x83 -- hash values for file data (without priority)
* 0x84 -- hash values for file data (with prioirty)

These are used to allow faster synchronisation by allowing a host to command other hosts to follow change sequences that are picked up through inotify.

* 0x90 -- file exists
* 0x91 -- create directory
* 0x92 -- truncate file
* 0x93 -- move out
* 0x94 -- move in
* 0x9f -- data -- a block of data that will go into a file

The following byte is always a variable length data byte which contains the length of the whole packet. The packet type alters the embedded data.


## Variable length data segment ##

A single byte which represents a data marker. The interpretation of the block depends on the value range:

* 0x00 to 0x7f represent a data block of that many bytes. Note that the command block (the following byte) is not counted either when the value is used as the packet header.
* 0x80 to 0xef command ID numbers (see above).
* 0xc0 to 0xf8 various fixed size blocks.
* 0xf9 to 0xff the following bytes represent the block size. The length in bytes of the size is the value minus 0xf8.

The data may be byte data (for a data block), or a string (for example in a create file event).

Short data segments thereby have a single byte header. A 200 byte string will have a value of 0xf9 (one size byte follows) and the next byte will have the value 200 (0xc8). The string content follows for 200 bytes.

A 1024 byte data block will have 0xfa (two size bytes follow) in the first byte, 0x04 in the next byte and then 0x00. Following this will be the 1024 bytes of data.


## Version packet `0x80` ##

* 8 bits -- version number. The highest protocol version number that this node understands.
* 32 bits -- (optional) server identity. A unique number used to identity the server. This number is randomly picked when a server first starts, but can be manually set in the server JSON database.
* 96 bits -- current time. The Lamport clock tick time that the server has.
* 256 bits -- (optional) server hash. The hash value describing all data that the server has across all nodes.


## Tenant packet `0x81` ##

* variable string -- tenant name.
* 256 bits -- tenant hash.


## Tenant hash packet `0x82` ##

* variable string -- tenant name.
* variable string -- hash prefix. This will be empty for the top level tenant hash. The layer number is always the length of this string.
* up to 32 copies of:
    * 8 bits:
        * least significant 5 bits -- hash suffix
        * most significant 3 bits -- zero
    * 256 bits -- hash value for the sub-part


## File hash packet without priority `0x83` ##

* variable string -- tenant name.
* variable string -- file name. Relative to the tenant root.


## File hash packet with priority `0x84` ##

* 96 bits -- time. The priority of the hash data
* variable string -- tenant name.
* variable string -- file name. Relative to the tenant root.


## File exists packet `0x90` ##

* 96 bits -- time. The priority of the file existence.
* variable -- tenant name.
* variable -- file name. Relative to the tenant root.
* 64 bits -- (optional) file size in bytes.
    * The most significant bit is always zero, giving a maximum file size of 7 EiB (I think). I.e. 63 bits of data are in the least significant bits with one bit of zero padding at the top.
* 256 bits -- (optional) hash value for the whole file content.

This packet does nothing other than ensure that a file exists. It does not signify anything about the file content at all (the content hash notwithstanding).

This packet can only be sent by a host that believes that it has the full correct version of the file due to the requirement to have a clock tick for the prority field.


## Create directory packet `0x91` ##

* 96 bits -- time. The priority of the directory.
* variable -- tenant name.
* variable -- directory name. Relative to the tenant root.


## Move inode out `0x93` ##

* 96 bits -- time. The priority of the directory.
* variable -- tenant name.
* variable -- directory name. Relative to the tenant root.


## File data block `0x9f` ##

* 96 bits -- time. The priority of the file data.
* variable -- tenant name.
* variable -- file path name. Relative to the tenant root.
* 64 bits -- byte offset. The offset of where this data block belongs.
* 256 bits -- SHA256 hash of the block data
* variable -- data bytes up to 32KB. All but the last block are always 32KB.


# Numeric analysis #

Looking at files first and assuming we go with 32KB data block sizes. A file with a bit tree depth of zero can have up to 32 blocks. This allows files up to 1MB in size to be described in a single hash data packet. A file of up to 32MB can now be described in 33 hash packets (one for the top level, and 32 to describe the actual file data). The tree depth is now 5 bits.

Adding an extra byte (so the file is 32MB + 1 byte in size) now increases the depth to 10 bits and we need 36 hash blocks (1 top level, two at the next level, and then 33 to describe the file content) as the top level must always be one hash.

Note that under this scheme the number of files in an entire sub-tree under a top level tracked directory is important, not the number of files at any given level.

The same sort of analysis works at the file level. A top level directory can contain up to 32 entries using a single hash block.

Adding an extra file or directory takes this up to 3 hashes (one for the top level and two to describe the 33 files). We don't need to add another hash until we exceed 64 files, and we grow at one hash block per 32 files until we get to 1024 files. At this point we need to add another level so we get a ten bit depth.

The case for the top level directories is exactly like the one for the tracked files and directories.

We can have up to 9,223,372,036,854,775,807 top level directories with up to the same number of file nodes in them and files of up to that size in bytes. Clearly no physical machines have this capacity.

More realistic numbers based on our MVP are that we wouldn't in practice need more than a handful of tracked top level directories. We want to represent up to about 10,000,000 files in a single top level directory. This is around 23 bits, so a 30 bit (six level, base 32) tree depth is more than sufficient.

This corresponds to 10,000,000 hashes for the individual files, plus 312,500 hashes for the next level of grouping, plus 9,766 for the next level, 306 for the next level 10 for the next level and one for the final level. There are 6 levels here which corresponds to 30 bits of hashes (6 times 5 bits). Total hashes are 10,322,583, which is around 315MB of data.

If files are small (4KB) then each file is fully specified by the existing hashes and no more are needed.  The total file data will be just under 38GB. This gives a hash overhead of less than one percent (about 0.8%). In practice the overhead will be more than this (we need to transport file names as well as file data and hashes), but it's hard to see that in real world use cases it'll be more than two or three times the hash overhead.

If the files are on average larger, say 1.5MB then each file would in addition require 48 hashes for the data and 2 hashes at the next level (the final top level hash is already counted above).

This gives an additional 500,000,000 hashes (50 per file across ten million files) for a total of around 15GB of hash data. The total file data size is now 14.3TB so the overhead is about 0.1%. Since a 32 byte hash covers a 32KB block, the data block hashes alone account for about one part in 1024 (~0.1%) of the file data, so this is a floor that no amount of extra tree fan-out can beat.
