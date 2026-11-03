#include "network/endpointUtils.h"
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

namespace Network
{
    std::string endpointToString(const Endpoint& endpoint)
    {
        char buffer[INET6_ADDRSTRLEN] = {};
        inet_ntop(endpoint.isIpv6 ? AF_INET6 : AF_INET, endpoint.ip, buffer, sizeof(buffer));

        std::ostringstream out;
        if (endpoint.isIpv6)
            out << '[' << buffer << ']';
        else
            out << buffer;
        out << ':' << endpoint.port;
        return out.str();
    }
}
