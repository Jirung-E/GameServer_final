#pragma once

#include <vector>
#include <queue>
#include <stdexcept>


struct Packet {
    unsigned char size;
    char type;
    char data[1024];

    Packet():
        size { 0 },
        type { 0 },
        data { }
    {

    }

    Packet(unsigned char size, char type):
        size { size },
        type { type },
        data { }
    {

    }
};


class PacketParser {
private:
    std::vector<char> bytes;
    std::queue<Packet> packets;

public:
    PacketParser() {
        bytes.reserve(1024);
    }

public:
    void push(char* data, size_t data_size) {
        if(data_size <= 0) {
            return;
        }

        size_t offset = 0;
        while(offset < data_size) {
            unsigned char packet_size = static_cast<unsigned char>(data[offset]);

            if(packet_size > data_size - offset) {
                for(size_t i=offset; i<data_size; ++i) {
                    bytes.push_back(data[i]);
                }
                break;
            }

            packets.emplace(packet_size, data[offset + 1]);
            memcpy(packets.back().data, data + offset + 2, packet_size - 2);

            offset += packet_size;
        }
    }
    Packet pop() {
        if(packets.empty()) {
            throw std::out_of_range("No packets to pop");
        }
        Packet packet = std::move(packets.front());
        packets.pop();
        return packet;
    }
    bool canPop() {
        return !packets.empty();
    }
};