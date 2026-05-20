#ifndef AUDIO_DEBUGGER_H
#define AUDIO_DEBUGGER_H

#include <vector>
#include <cstdint>

#include <sys/socket.h>
#include <netinet/in.h>

// #define CONFIG_USE_AUDIO_DEBUGGER 1
// #define CONFIG_AUDIO_DEBUG_UDP_SERVER "10.242.174.153:8000"

class AudioDebugger {
public:
    AudioDebugger();
    ~AudioDebugger();

    void Feed(const std::vector<int16_t>& data);

    void Feed(const std::vector<uint8_t>& payload);
private:
    int udp_sockfd_ = -1;
    struct sockaddr_in udp_server_addr_;
};

#endif 