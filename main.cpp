#include "Log.h"
#include "TimeReference.h"
#include "Application.h"
#include "webserver/Webserver.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, const char* argv[]) try
{
    Log::setGlobalLoggingLevel(debug);
    Log::setChannelLoggingLevel("webserver", info);
    Log::setChannelLoggingLevel("websocket", info);

    Application& app = Application::instance();

    po::options_description options("Generic options");
    options.add_options()
            ("help,h", "produce help message")
            ("dbhost", po::value(&app.options.db_hostname), "database host name")
            ("dbport", po::value(&app.options.db_port), "database port")
            ("dbusername", po::value(&app.options.db_username), "database user name")
            ("dbpassword", po::value(&app.options.db_password), "database password")
            ("httpport", po::value<unsigned short>(), "server port")
            ("simspeed", po::value<unsigned int>(), "initial simulation speed")
            ("log-level", po::value<std::string>(), "set logging level [trace|debug|info|warning|error]")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << options << "\n";
        exit(EXIT_SUCCESS);
    }

    if (vm.count("log-level")) {
        Log::setGlobalLoggingLevel(vm["log-level"].as<std::string_view>());
    }

    Log("main", info) << "Application started";

    // Application init
    app.init();

    // Simulator initial speed
    if (vm.count("simspeed"))
        TimeReference::instance().setSpeed(vm["simspeed"].as<unsigned>());
    else
        TimeReference::instance().setSpeed(3600 * 24);

    // Clear database
    app.cleanDatabase();

    // Start webserver
    unsigned short port = vm.count("httpport") ? vm["httpport"].as<unsigned short>() : 8080;
    return Webserver().run(port);
}
catch (std::exception& e) {
    Log("main", error) << e.what();
}