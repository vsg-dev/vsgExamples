
#include <iostream>

#include "Packet.h"


//////////////////////////////////////////////////////////////////////////////////////
//
// Packet
//
Packet::Packet()
{
    std::cout<<"Packet() "<< this<<std::endl;
}

Packet::~Packet()
{
    std::cout<<"~Packet() "<< this<<std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// Packets
//
void Packets::clear()
{
    for(auto& packet : packets)
    {
        pool.emplace(std::move(packet.second));
    }
    packets.clear();
}

void Packets::copy(const std::string& str)
{
    clear();

    uint32_t packetIndex = 0;

    std::size_t i = 0;
    std::size_t totalSize = str.size();

    while(i < totalSize)
    {
        std::unique_ptr<Packet> packet;
        if (pool.empty())
        {
            packet = std::unique_ptr<Packet>(new Packet);
        }
        else
        {
            packet = std::move(pool.top());
            pool.pop();
        }

        std::size_t remaining = totalSize - i;

        packet->packetIndex = packetIndex;
        packet->packetSize = (remaining < DATA_SIZE) ? remaining : DATA_SIZE;

        for(std::size_t j = 0; j < packet->packetSize; ++j, ++i)
        {
            packet->data[j] = str[i];
        }

        packets[packetIndex] = std::move(packet);
        ++packetIndex;
    }

    for(auto& packet : packets)
    {
        packet.second->packetCount = packetIndex;
        packet.second->totalSize = totalSize;
    }
}

std::string Packets::assemble() const
{
    if (packets.empty())
    {
        return {};
    }

    std::size_t i = 0;
    std::size_t totalSize = packets.begin()->second->totalSize;

    std::string str(totalSize, '\0');

    for(auto& packet : packets)
    {
        for(std::size_t j=0; j<packet.second->packetSize; ++j, ++i)
        {
            str[i] = packet.second->data[j];
        }
    }

    return str;
}

