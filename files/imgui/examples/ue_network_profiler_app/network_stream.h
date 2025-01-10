//
// Created by captainchen on 2025/1/5.
//

#ifndef EXAMPLE_PROFILER_APP_NETWORK_STREAM_H
#define EXAMPLE_PROFILER_APP_NETWORK_STREAM_H
#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "stream_header.h"

class PartialNetworkStream;

/** Enum values need to be in sync with UE3 */
enum class EChannelTypes {
    Invalid = 0,    // Invalid type.
    Control,        // Connection control.
    Actor,          // Actor-update channel.
    File,           // Binary file transfer.
    Voice,          // Voice channel
    Max
};

class NetworkStream {
public:
    /** Per packet overhead to take into account for total outgoing bandwidth. */
    static const int PacketOverhead = 28;

    /** Array of unique names. Code has fixed indexes into it. */
    std::vector<std::string> NameArray;

    /** Array of unique addresses. Code has fixed indexes into it. */
    std::vector<uint64_t> AddressArray;

    /** Used for new storage method. */
    std::vector<std::string> StringAddressArray;

    //Connection对应的PlayerName
    std::unordered_map<int,std::string> ConnectionPlayerNameMap;

    /** Last address index parsed from token stream */
    int CurrentConnectionIndex = 0;

    /** Internal dictionary from class name to index in name array, used by GetClassNameIndex. */
    std::unordered_map<std::string, int> ClassNameToNameIndex;

    /** Index of "Unreal" name in name array. */
    int NameIndexUnreal = -1;

    /**
     * At the highest level, the entire stream is a series of frames.
     * 个PartialNetworkStream对象的列表，表示整个流的帧序列。PartialNetworkStream是NetworkStream的一个内部类，存储着一帧的数据。
     */
    std::vector<PartialNetworkStream*> Frames;

    /** NetworkProfiler写入的文件头 */
    StreamHeader Header;

    /** Returns the name associated with the passed in index. */
    std::string GetName(int Index) const {
        return NameArray[Index];
    }

    /** Returns the ip address string associated with the passed in connection index. */
    std::string GetIpString(int ConnectionIndex, uint32_t NetworkVersion) const {
        if (NetworkVersion < 12) {
            uint64_t Addr = AddressArray[ConnectionIndex];
            uint32_t IP = static_cast<uint32_t>(Addr >> 32);
            uint32_t Port = static_cast<uint32_t>(Addr & ((static_cast<uint64_t>(1) << 32) - 1));

            return std::to_string((IP >> 24) & 255) + "." +
                   std::to_string((IP >> 16) & 255) + "." +
                   std::to_string((IP >> 8) & 255) + "." +
                   std::to_string(IP & 255) + ": " +
                   std::to_string(Port);
        } else {
            return StringAddressArray[ConnectionIndex];
        }
    }

    /** Returns the class name index for the passed in actor name index */
    int GetClassNameIndex(int ActorNameIndex) {
        int ClassNameIndex = 0;
        try {
            std::string ActorName = GetName(ActorNameIndex);
            std::string ClassName = ActorName;

            size_t CharIndex = ActorName.find_last_of('_');
            if (CharIndex != std::string::npos) {
                ClassName = ActorName.substr(0, CharIndex);
            }

            if (ClassNameToNameIndex.find(ClassName) != ClassNameToNameIndex.end()) {
                ClassNameIndex = ClassNameToNameIndex[ClassName];
            } else {
                ClassNameIndex = NameArray.size();
                NameArray.push_back(ClassName);
                ClassNameToNameIndex[ClassName] = ClassNameIndex;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error Parsing ClassName for Actor: " << ActorNameIndex << " " << e.what() << std::endl;
        }

        return ClassNameIndex;
    }

    /** Returns the class name index for the passed in actor name */
    int GetIndexFromClassName(const std::string& ClassName) const {
        auto it = ClassNameToNameIndex.find(ClassName);
        if (it != ClassNameToNameIndex.end()) {
            return it->second;
        }
        return -1;
    }

    uint32_t GetVersion() const {
        return Header.Version;
    }

    std::string EnumToString(EChannelTypes types) const {
        return std::string("not support");
    }

/** Returns the channel type name for the passed in index. */
    std::string GetChannelTypeName(int ChannelTypeIndex) const {
        uint32_t Version = GetVersion();

        if (Version < 11) {
            return EnumToString(EChannelTypes(ChannelTypeIndex));
        } else {
            return GetName(ChannelTypeIndex);
        }
    }
};


#endif //EXAMPLE_PROFILER_APP_NETWORK_STREAM_H
