#pragma once

#include <map>
#include <stack>
#include <memory>

#include "Broadcaster.h"
#include "Receiver.h"


const uint64_t DATA_SIZE = 32768 - 40;

struct Packet
{
    Packet();
    ~Packet();

    struct Header
    {
        uint64_t set = 0;

        uint64_t totalSize = 0;
        uint32_t packetCount = 0;

        uint32_t packetIndex = 0;
        uint64_t packetSize = 0;

        uint64_t hash = 0;
    } header;

    uint8_t data[DATA_SIZE];
};

struct Packets
{
    uint64_t set = 0;
    std::map<uint32_t, std::unique_ptr<Packet>> packets;
    std::stack<std::unique_ptr<Packet>> pool;

    std::unique_ptr<Packet> createPacket()
    {
        if (pool.empty())
        {
            return std::unique_ptr<Packet>(new Packet);
        }
        else
        {
            std::unique_ptr<Packet> packet = std::move(pool.top());
            pool.pop();
            return packet;
        }
    }

    void clear();
    void copy(const std::string& str);
    std::string assemble() const;
};

struct PacketBroadcaster
{
    vsg::ref_ptr<Broadcaster> broadcaster;

    Packets packets;

    void broadcast(uint64_t set, vsg::ref_ptr<vsg::Object> object);
};

struct PacketReceiver
{
    vsg::ref_ptr<Receiver> receiver;

    Packets packets;

    vsg::ref_ptr<vsg::Object> receive();
};
