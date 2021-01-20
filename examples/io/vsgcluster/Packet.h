#pragma once

#include <map>
#include <stack>
#include <memory>

const uint64_t DATA_SIZE = 28;

struct Packet
{
    Packet();
    ~Packet();

    uint64_t set = 0;

    uint64_t totalSize = 0;
    uint32_t packetCount = 0;

    uint32_t packetIndex = 0;
    uint64_t packetSize = 0;

    uint64_t hash = 0;

    uint8_t data[DATA_SIZE];
};

struct Packets
{
    uint64_t set = 0;
    std::map<uint32_t, std::unique_ptr<Packet>> packets;
    std::stack<std::unique_ptr<Packet>> pool;

    void clear();
    void copy(const std::string& str);
    std::string assemble() const;
};

