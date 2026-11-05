#include <csignal>
#include <cstdlib>
#include "server/server.h"

/*
    Universidade Tecnológica Federal do Paraná - UTFPR
    Departamento Acadêmico de Informática - DAINF
    ICSR30 - Redes de Computadores

    Trabalho de Programação 2

    Lucas Maciel Ferreira - 2556596
*/ 

static Server::Server* runningServer = nullptr;

static void onSignal(int)
{
    if (runningServer != nullptr)
        runningServer->stop();
}

int main()
{
    Server::Server server;
    runningServer = &server;

    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    server.start();

    std::_Exit(0);
}
