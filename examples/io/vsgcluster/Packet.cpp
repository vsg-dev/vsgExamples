
#include <iostream>
#include <sstream>

#include "Packet.h"

#include <vsg/io/VSG.h>

//////////////////////////////////////////////////////////////////////////////////////
//
// Packet
//
Packet::Packet()
{
    //    std::cout<<"Packet() "<< this<<std::endl;
}

Packet::~Packet()
{
    //    std::cout<<"~Packet() "<< this<<std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// Packets
//
void PacketSet::clear()
{
    for (auto& packet : packets)
    {
        pool.emplace(std::move(packet.second));
    }
    packets.clear();
}

bool PacketSet::add(std::unique_ptr<Packet> packet)
{
    bool complete = (packets.size() + 1) == packet->header.packetCount;

    packets[packet->header.packetIndex] = std::move(packet);

    return complete;
}

void PacketSet::copy(const std::string& str)
{
    clear();

    uint32_t packetIndex = 0;

    std::size_t i = 0;
    std::size_t totalSize = str.size();

    while (i < totalSize)
    {
        auto packet = createPacket();

        std::size_t remaining = totalSize - i;

        packet->header.packetIndex = packetIndex;
        packet->header.packetSize = (remaining < DATA_SIZE) ? remaining : DATA_SIZE;

        for (std::size_t j = 0; j < packet->header.packetSize; ++j, ++i)
        {
            packet->data[j] = str[i];
        }

        packets[packetIndex] = std::move(packet);
        ++packetIndex;
    }

    for (auto& packet : packets)
    {
        packet.second->header.packetCount = packetIndex;
        packet.second->header.totalSize = totalSize;
    }
}

std::string PacketSet::assemble() const
{
    if (packets.empty())
    {
        return {};
    }

    std::size_t i = 0;
    std::size_t totalSize = packets.begin()->second->header.totalSize;

    std::string str(totalSize, '\0');

    for (auto& packet : packets)
    {
        for (std::size_t j = 0; j < packet.second->header.packetSize; ++j, ++i)
        {
            str[i] = packet.second->data[j];
        }
    }

    return str;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// PacketBroadcaster
//
void PacketBroadcaster::broadcast(uint64_t set, vsg::ref_ptr<vsg::Object> object)
{
    auto options = vsg::Options::create();
    options->extensionHint = "vsgb";

    std::ostringstream ostr(std::ios::out | std::ios::binary);
    vsg::VSG rw;
    rw.write(object, ostr, options);

    packets.copy(ostr.str());

    for (auto& packet : packets.packets)
    {
        Packet& ref = *packet.second;
        ref.header.set = set;
        std::size_t size = sizeof(Packet::Header) + ref.header.packetSize;
        broadcaster->broadcast(&ref, static_cast<unsigned int>(size));
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//
// PacketReciever
//
std::unique_ptr<Packet> PacketReceiver::createPacket()
{
    if (!packetPool.empty())
    {
        std::unique_ptr<Packet> packet = std::move(packetPool.top());
        packetPool.pop();
        return packet;
    }
#if 0
    for(auto& packetSet : packetSetPool)
    {
        auto packet = packetSet->takePacketFromPool();
        if (packet)
        {
            std::cout<<"PacketReceiver::createPacket() taken from inactive pool"<<std::endl;
            return packet;
        }
    }
#endif
    for (auto& packetSet : packetSetMap)
    {
        auto packet = packetSet.second->takePacketFromPool();
        if (packet)
        {
            // std::cout<<"PacketReceiver::createPacket() taken from active pool"<<std::endl;
            return packet;
        }
    }

    // std::cout<<"PacketReceiver::createPacket() created new Packet"<<std::endl;
    return std::unique_ptr<Packet>(new Packet);
}

vsg::ref_ptr<vsg::Object> PacketReceiver::completed(uint64_t set)
{
    auto set_itr = packetSetMap.find(set);
    if (set_itr == packetSetMap.end()) return {};

    // convert the PacketSet into a vsg::Object
    std::istringstream istr((set_itr->second)->assemble());
    vsg::VSG rw;
    auto object = rw.read(istr);

    // clean up the PacketSet
    auto next_itr = set_itr;
    ++next_itr;

    for (auto itr = packetSetMap.begin(); itr != next_itr; ++itr)
    {
        itr->second->clear();
        packetSetPool.push(std::move(itr->second));
    }

    packetSetMap.erase(packetSetMap.begin(), next_itr);

    return object;
}

bool PacketReceiver::add(std::unique_ptr<Packet> packet)
{
    uint64_t set = packet->header.set;

    if (packetSetMap.count(set) == 0)
    {
        // packet applies to a new set.
        // need to get a PacketSet from the pool if one is available.
        if (!packetSetPool.empty())
        {
            //std::cout<<"Reusing a PacketSet from the pool."<<std::endl;
            packetSetMap[set] = std::move(packetSetPool.top());
            packetSetPool.pop();
        }
        else
        {
            //std::cout<<"Creating a new PacketSet."<<std::endl;
            packetSetMap[set] = std::unique_ptr<PacketSet>(new PacketSet);
        }
    }
    return packetSetMap[set]->add(std::move(packet));
}

vsg::ref_ptr<vsg::Object> PacketReceiver::receive()
{
    auto first_packet = createPacket();
    unsigned int first_size = receiver->receive(&(*first_packet), sizeof(Packet));
    if (first_size == 0)
    {
        packetPool.emplace(std::move(first_packet));
        return {};
    }

    uint64_t set = first_packet->header.set;
    if (add(std::move(first_packet)))
    {
        return completed(set);
    }

    while (true)
    {
        auto packet = createPacket();
        unsigned int size = receiver->receive(&(*packet), sizeof(Packet));
        if (size == 0)
        {
            packetPool.emplace(std::move(packet));
            return {};
        }

        set = packet->header.set;
        if (add(std::move(packet)))
        {
            return completed(set);
        }
    }

    return {};
}
