
#include <iostream>
#include <sstream>

#include "Packet.h"

#include <vsg/io/ReaderWriter_vsg.h>

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
        auto packet = createPacket();

        std::size_t remaining = totalSize - i;

        packet->header.packetIndex = packetIndex;
        packet->header.packetSize = (remaining < DATA_SIZE) ? remaining : DATA_SIZE;

        for(std::size_t j = 0; j < packet->header.packetSize; ++j, ++i)
        {
            packet->data[j] = str[i];
        }

        packets[packetIndex] = std::move(packet);
        ++packetIndex;
    }

    for(auto& packet : packets)
    {
        packet.second->header.packetCount = packetIndex;
        packet.second->header.totalSize = totalSize;
    }
}

std::string Packets::assemble() const
{
    if (packets.empty())
    {
        return {};
    }

    std::size_t i = 0;
    std::size_t totalSize = packets.begin()->second->header.totalSize;

    std::string str(totalSize, '\0');

    for(auto& packet : packets)
    {
        for(std::size_t j=0; j<packet.second->header.packetSize; ++j, ++i)
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
    std::ostringstream ostr;
    vsg::ReaderWriter_vsg rw;
    rw.write(object, ostr);

    packets.copy(ostr.str());

    for(auto& packet : packets.packets)
    {
        Packet& ref = *packet.second;
        ref.header.set = set;
        std::size_t size = sizeof(Packet::Header) + ref.header.packetSize;
        broadcaster->broadcast(&ref, size);
    }
}


//////////////////////////////////////////////////////////////////////////////////////
//
// PacketReciever
//
vsg::ref_ptr<vsg::Object> PacketReceiver::receive()
{
    std::cout<<"\nPacketReceiver::receive()"<<std::endl;

    packets.clear();

    auto packet = packets.createPacket();

    unsigned int size = receiver->recieve(&(*packet), sizeof(Packet));
    if (size == 0)
    {
        packets.pool.emplace(std::move(packet));
        return {};
    }

    uint32_t packetCount = packet->header.packetCount;

    std::cout<<"    assinging to "<<packet->header.packetIndex<<std::endl;
    packets.packets[packet->header.packetIndex] = std::move(packet);

    while (packets.packets.size() < packetCount)
    {
        auto additional_packet = packets.createPacket();

        unsigned int size = receiver->recieve(&(*additional_packet), sizeof(Packet));
        if (size == 0)
        {
            packets.pool.emplace(std::move(additional_packet));
            return {};
        }

        std::cout<<"    assinging to "<<additional_packet->header.packetIndex<<std::endl;

        packets.packets[additional_packet->header.packetIndex] = std::move(additional_packet);

    }

    if (packets.packets.size() == packetCount)
    {
        std::istringstream istr(packets.assemble());
        vsg::ReaderWriter_vsg rw;
        return rw.read(istr);
    }

    return {};
}
