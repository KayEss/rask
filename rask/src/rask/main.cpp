/*
    Copyright 2015, Proteus Tech Co Ltd. http://www.kirit.com/Rask
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include <beanbag/beanbag>
#include <fost/internet>
#include <fost/http.server.hpp>
#include <fost/log>
#include <fost/main>
#include <fost/urlhandler>

#include <rask/peer.hpp>
#include <rask/server.hpp>
#include <rask/tenants.hpp>
#include <rask/workers.hpp>


namespace {


    const fostlib::setting<fostlib::json> c_logger(
        "rask/main.cpp", "rask", "logging", fostlib::json(), true);
    const fostlib::setting<fostlib::json> c_peers_db(
        "rask/main.cpp", "rask", "peers", fostlib::json(), true);
    const fostlib::setting<fostlib::json> c_server_db(
        "rask/main.cpp", "rask", "server", fostlib::json(), true);
    const fostlib::setting<fostlib::json> c_tenant_db(
        "rask/main.cpp", "rask", "tenants", fostlib::json(), true);
    const fostlib::setting<uint16_t> c_webserver_port(
        "rask/main.cpp", "rask", "webserver-port", 4000, true);

    // Take out the Fost logger configuration so we don't end up with both
    const fostlib::setting<fostlib::json> c_fost_logger(
        "rask/main.cpp", "rask", "Logging sinks", fostlib::json::parse(
            "{\"sinks\":[]}"));


}


FSL_MAIN("rask", "Rask")(fostlib::ostream &out, fostlib::arguments &args) {
    // Load the configuration files we've been given on the command line
    std::vector<fostlib::settings> configuration;
    configuration.reserve(args.size());
    for ( std::size_t arg{1}; arg != args.size(); ++arg ) {
        auto filename = fostlib::coerce<boost::filesystem::path>(args[arg].value());
        out << "Loading config " << filename << std::endl;
        configuration.emplace_back(filename);
    }
    // TODO: Handle extra switches
    // Set up the logging options
    std::unique_ptr<fostlib::log::global_sink_configuration> loggers;
    if ( !c_logger.value().isnull() && c_logger.value().has_key("sinks") ) {
        loggers = std::make_unique<fostlib::log::global_sink_configuration>(c_logger.value());
    }
    // Start the threads for doing work
    rask::workers workers;
    // Work out server identity
    if ( !c_server_db.value().isnull() ) {
        beanbag::jsondb_ptr dbp(beanbag::database(c_server_db.value()["database"]));
        fostlib::jsondb::local server(*dbp);
        if ( !server.has_key("identity") ) {
            uint32_t random = 0;
            std::ifstream urandom("/dev/urandom");
            random += urandom.get() << 16;
            random += urandom.get() << 8;
            random += urandom.get();
            random &= (1 << 20) - 1; // Take 20 bits
            server.set("identity", random);
            server.commit();
            fostlib::log::info()("Server identity picked as", random);
        }
        // Start listening for connections
        rask::listen(workers, c_server_db.value()["socket"]);
    }
    // Load tenants and start sweeping
    if ( !c_tenant_db.value().isnull() ) {
        rask::tenants(workers, c_tenant_db.value());
        workers.notify();
    }
    // Connect to peers
    if ( !c_peers_db.value().isnull() ) {
        rask::peer(workers, c_peers_db.value());
    }
    // All done, finally start the web server (or whatever)
    if ( c_webserver_port.value() ) {
        // Log that we've started
        fostlib::log::debug("Started Rask, spinning up web server");
        // Spin up the web server
        fostlib::http::server server(fostlib::host(), c_webserver_port.value());
        server(fostlib::urlhandler::service); // This will never return
    } else {
        // Log that we're sleeping
        fostlib::log::warning("Started Rask without a webserver -- sleeping for 10 seconds");
        sleep(10);
    }
    return 0;
}
