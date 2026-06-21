# Rask #

Rask is (going to be) a multi-way peer-to-peer low-latency file replication system. Rask is the Norwegian word for "fast".


## Motivating use case ##

We have a number of web application servers serving a web site that includes a file upload feature. When a file has been updated to one server the next request issued by the browser may hit another application server, and this server should be able to serve the just uploaded file.


### Problems with existing solutions ###

* Use a networked file system

    We could try to solve this using something like NFS, but there is no good way to make NFS redundant. When the server goes down then we lose the share and all application servers are affected.

* Use rsync in a CRON job

    This would be both secure and fast to do the actual file transfers, but the latency would be too high to allow another application server to serve a newly uploaded file.

    
## Minimal Viable Product ##

We're aiming for a very limited set of features for the first version so we have just enough to match our motivating use case.

* Sweep feature to do an initial sync of the folders which will result in a super-set of the files across all nodes. Deletes will only propagate when the Rask monitors a file delete event on a node.
* Binary protocol to allow prioritised sends of data and instructions to another Rask node.
* Library form of the core functionality so other applications can be built on top of Rask.
* Use the kernel [inotify](http://man7.org/linux/man-pages/man7/inotify.7.html) stream on the watched directories in order to get the latency down as much as possible.
* Use io_uring for file and network I/O. This project will be Linux only, and only usable where io_uring is also available.
* Keep a database of meta-data to speed up resynchronisation after a network break between nodes.
* Be able to scale up to ten thousand directories, one million files per directory and up to ten million files overall.
* We will aim for network latency & bandwidth and disk speeds to be the dominant factors in the speed of file replication. Rask itself should have very low CPU load when not doing anything.
* Surface synchronisation activity and counters by logging to the terminal.

The MVP will not:

* Allow changes of configuration after loading. The server will need to be restarted for changes to take effect.
* Attempt to synchronise file attributes.
* It will not try to efficiently send changes to a file.
* We will not try to optimise memory usage or disk usage for Rask.
* We will not secure the connections between nodes -- they will be neither authenticated or encrypted. If a deployment requires nodes to communicate over a non-trusted and insecure network (e.g. the Internet) the a VPN should be used so the nodes can assume safe transit.
* We will not provide an embedded web server or HTTP/JSON monitoring API. The MVP relies on terminal logs; remote/programmatic monitoring is deferred.

All of these can be features once the MVP has been released.
