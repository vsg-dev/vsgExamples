#pragma once

#include <map>
#include <memory>
#include <stack>

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

struct PacketSet
{
    uint64_t set = 0;
    std::map<uint32_t, std::unique_ptr<Packet>> packets;
    std::stack<std::unique_ptr<Packet>> pool;

    std::unique_ptr<Packet> takePacketFromPool()
    {
        if (!pool.empty())
        {
            std::unique_ptr<Packet> packet = std::move(pool.top());
            pool.pop();
            return packet;
        }
        else
        {
            return {};
        }
    }

    std::unique_ptr<Packet> createPacket()
    {
        auto packet = takePacketFromPool();
        if (packet)
            return packet;
        else
            return std::unique_ptr<Packet>(new Packet);
    }

    void clear();
    bool add(std::unique_ptr<Packet> packet);

    void copy(const std::string& str);
    std::string assemble() const;
};

struct PacketBroadcaster
{
    vsg::ref_ptr<Broadcaster> broadcaster;

    PacketSet packets;

    void broadcast(uint64_t set, vsg::ref_ptr<vsg::Object> object);
};

struct PacketReceiver
{
    vsg::ref_ptr<Receiver> receiver;

    std::map<uint64_t, std::unique_ptr<PacketSet>> packetSetMap;

    std::stack<std::unique_ptr<Packet>> packetPool;
    std::stack<std::unique_ptr<PacketSet>> packetSetPool;

    std::unique_ptr<Packet> createPacket();
    bool add(std::unique_ptr<Packet> packet);

    vsg::ref_ptr<vsg::Object> completed(uint64_t set);
    vsg::ref_ptr<vsg::Object> receive();
};
