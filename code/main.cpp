#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>

#include "server/webserver.h"
#include "config/config.h"

static WebServer* g_server = nullptr;
void signal_handler(int sig) {
    if(sig == SIGINT) {
        if(g_server) {
            g_server->stop();
        }
    }
}
int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    Config::getInstance().parse_config_file("config.conf");
    if(argc > 1) 
        Config::getInstance().parse_args(argc, argv);
    WebServer server;
    g_server = &server;
    server.start();
    g_server = nullptr;
    return 0;
}
