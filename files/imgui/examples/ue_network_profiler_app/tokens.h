//
// Created by captainchen on 2025/1/5.
//

#ifndef EXAMPLE_PROFILER_APP_TOKENS_H
#define EXAMPLE_PROFILER_APP_TOKENS_H

#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <unordered_set>
#include "network_stream.h"

class TreeNodeCollection;

class FilterValues
{
public:
    std::string ActorFilter = "";
    std::string PropertyFilter = "";
    std::string RPCFilter = "";

    std::unordered_set<int> ConnectionMask;
};

class TokenHelper {
public:
    static const int NumBitsPerDWord = 32;
    static const int NumBitsPerDWordLog2 = 5;

    static int LoadPackedInt(std::ifstream& BinaryStream) {
        uint32_t Value = 0;
        uint8_t cnt = 0;
        bool more = true;
        while (more) {
            uint32_t NextByte = BinaryStream.get();

            more = (NextByte & 1) != 0;  // Check 1 bit to see if there's more after this
            NextByte = NextByte >> 1;    // Shift to get actual 7 bit value
            Value += NextByte << (7 * cnt++);  // Add to total value
        }

        return static_cast<int>(Value);
    }

    static void ReadBitArray(std::ifstream& BinaryStream, std::bitset<NumBitsPerDWord>& OutBitArray) {
        // TODO: Verify Endianness
        int NumBits = LoadPackedInt(BinaryStream);
        int NumInts = ((NumBits + NumBitsPerDWord - 1) >> NumBitsPerDWordLog2);
        std::vector<int> ReadValues(NumInts);

        for (int32_t Idx = 0; Idx < NumInts; Idx++) {
            ReadValues[Idx] = LoadPackedInt(BinaryStream);
        }

        OutBitArray = std::bitset<NumBitsPerDWord>(ReadValues.data(), NumBits);
    }
};

enum class ETokenTypes {
    FrameMarker = 0,
    SocketSendTo,
    SendBunch,
    SendRPC,
    ReplicateActor,
    ReplicateProperty,
    EndOfStreamMarker,
    Event,
    RawSocketData,
    SendAck,
    WritePropertyHeader,
    ExportBunch,
    MustBeMappedGuids,
    BeginContentBlock,
    EndContentBlock,
    WritePropertyHandle,
    ConnectionChange,
    NameReference,
    ConnectionReference,
    PropertyComparison,
    ReplicatePropertiesMetaData,
    ConnectionSaturated,
    ConnectionQueuedBits,
    ConnectionNetLodLevel,
    MaxAndInvalid
};

class TokenBase {
public:
    ETokenTypes TokenType = ETokenTypes::MaxAndInvalid;
    int ConnectionIndex = 0;

    static TokenBase* ReadNextToken(std::ifstream& BinaryStream, NetworkStream& InNetworkStream);

    ETokenTypes GetTokenType() const {
        return TokenType;
    }

    virtual void ToDetailedTreeView() {
        // Implementation needed
    }

    virtual void ToDetailedTreeView(TreeNodeCollection& List, const FilterValues& InFilterValues)
    {

    }

    virtual bool MatchesFilters() {
        // Implementation needed
        return true;
    }

    virtual bool MatchesFilters(const FilterValues& InFilterValues) const
    {
        if (TokenType == ETokenTypes::FrameMarker || TokenType == ETokenTypes::EndOfStreamMarker)
        {
            return true;
        }

        return InFilterValues.ConnectionMask.empty() || InFilterValues.ConnectionMask.find(ConnectionIndex) != InFilterValues.ConnectionMask.end();
    }
};

class TokenFrameMarker : public TokenBase {
public:
    /** Relative time of frame since start of engine. */
    float RelativeTime;

    /** Constructor, serializing members from passed in stream. */
    TokenFrameMarker(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&RelativeTime), sizeof(RelativeTime));
    }

    void ToDetailedTreeView() override {
        // Assuming TreeNode and TokenHelper are defined elsewhere

    }
};

class TokenEndOfStreamMarker : public TokenBase {
};

class TokenSocketSendTo : public TokenBase {
public:
    /** Socket debug description name index. "Unreal" is special name for game traffic. */
    int SocketNameIndex;
    /** Bytes actually sent by low level code. */
    uint16_t BytesSent;
    /** Number of bits representing the packet id */
    uint16_t NumPacketIdBits;
    /** Number of bits representing bunches */
    uint16_t NumBunchBits;
    /** Number of bits representing acks */
    uint16_t NumAckBits;
    /** Number of bits used for padding */
    uint16_t NumPaddingBits;

    /** Constructor, serializing members from passed in stream. */
    TokenSocketSendTo(std::ifstream& BinaryStream) {
        SocketNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&BytesSent), sizeof(BytesSent));
        BinaryStream.read(reinterpret_cast<char*>(&NumPacketIdBits), sizeof(NumPacketIdBits));
        BinaryStream.read(reinterpret_cast<char*>(&NumBunchBits), sizeof(NumBunchBits));
        BinaryStream.read(reinterpret_cast<char*>(&NumAckBits), sizeof(NumAckBits));
        BinaryStream.read(reinterpret_cast<char*>(&NumPaddingBits), sizeof(NumPaddingBits));
    }

    void ToDetailedTreeView() override {

    }

    bool MatchesFilters() override {
        return TokenBase::MatchesFilters();
    }
};

class TokenSendBunch : public TokenBase {
public:
    /** Channel index. */
    uint16_t ChannelIndex;
    /** Channel type. */
    uint8_t ChannelType;
    /** Channel type name index. */
    int ChannelTypeNameIndex;
    /** Number of header bits serialized/sent. */
    uint16_t NumHeaderBits;
    /** Number of non-header bits serialized/sent. */
    uint16_t NumPayloadBits;

    /** Constructor, serializing members from passed in stream. */
    TokenSendBunch(std::ifstream& BinaryStream, uint32_t Version) {
        BinaryStream.read(reinterpret_cast<char*>(&ChannelIndex), sizeof(ChannelIndex));
        if (Version < 11) {
            BinaryStream.read(reinterpret_cast<char*>(&ChannelType), sizeof(ChannelType));
            ChannelTypeNameIndex = -1;
        } else {
            ChannelType = 0;
            ChannelTypeNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        }
        BinaryStream.read(reinterpret_cast<char*>(&NumHeaderBits), sizeof(NumHeaderBits));
        BinaryStream.read(reinterpret_cast<char*>(&NumPayloadBits), sizeof(NumPayloadBits));
    }

    int GetChannelTypeIndex() const {
        return (ChannelTypeNameIndex != -1) ? ChannelTypeNameIndex : ChannelType;
    }

    /** Gets the total number of bits serialized for the bunch. */
    int GetNumTotalBits() const {
        return NumHeaderBits + NumPayloadBits;
    }
};

class TokenSendRPC : public TokenBase {
public:
    /** Name table index of actor name. */
    int ActorNameIndex;
    /** Name table index of function name. */
    int FunctionNameIndex;
    /** Number of bits serialized/sent for the header. */
    uint16_t NumHeaderBits;
    /** Number of bits serialized/sent for the parameters. */
    uint16_t NumParameterBits;
    /** Number of bits serialized/sent for the footer. */
    uint16_t NumFooterBits;

    /** Constructor, serializing members from passed in stream. */
    TokenSendRPC(std::ifstream& BinaryStream, uint32_t Version) {
        ActorNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        FunctionNameIndex = TokenHelper::LoadPackedInt(BinaryStream);

        if (Version < 13) {
            BinaryStream.read(reinterpret_cast<char*>(&NumHeaderBits), sizeof(NumHeaderBits));
            BinaryStream.read(reinterpret_cast<char*>(&NumParameterBits), sizeof(NumParameterBits));
            BinaryStream.read(reinterpret_cast<char*>(&NumFooterBits), sizeof(NumFooterBits));
        } else {
            NumHeaderBits = TokenHelper::LoadPackedInt(BinaryStream);
            NumParameterBits = TokenHelper::LoadPackedInt(BinaryStream);
            NumFooterBits = TokenHelper::LoadPackedInt(BinaryStream);
        }
    }

    /** Gets the total number of bits serialized for the RPC. */
    int GetNumTotalBits() const {
        return NumHeaderBits + NumParameterBits + NumFooterBits;
    }

    std::string& GetDescription();

public:
    std::string description_;
};


class TokenReplicateProperty : public TokenBase {
public:
    /** Name table index of property name. */
    int PropertyNameIndex;
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenReplicateProperty(std::ifstream& BinaryStream) {
        PropertyNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenWritePropertyHeader : public TokenBase {
public:
    /** Name table index of property name. */
    int PropertyNameIndex;
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenWritePropertyHeader(std::ifstream& BinaryStream) {
        PropertyNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenReplicateActor : public TokenBase {
public:
    enum ENetFlags {
        Dirty = 1,
        Initial = 2,
        Owner = 4
    };

    /** Whether bNetDirty, bNetInitial, or bNetOwner was set on Actor. */
    uint8_t NetFlags;
    /** Name table index of actor name */
    int ActorNameIndex;
    /** Time in ms to replicate this actor */
    float TimeInMS;

    /** List of property tokens that were serialized for this actor. */
    std::vector<TokenReplicateProperty> Properties;

    /** List of property header tokens that were serialized for this actor. */
    std::vector<TokenWritePropertyHeader> PropertyHeaders;

    /** Constructor, serializing members from passed in stream. */
    TokenReplicateActor(std::ifstream& BinaryStream) {
        NetFlags = BinaryStream.get();
        ActorNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&TimeInMS), sizeof(TimeInMS));
    }

    /**
     * Returns the number of bits for this replicated actor while taking filters into account.
     */
    int GetNumReplicatedBits() const {
        int NumReplicatedBits = 0;
        for (const auto& Property : Properties) {
            NumReplicatedBits += Property.NumBits;
        }

        for (const auto& PropertyHeader : PropertyHeaders) {
            NumReplicatedBits += PropertyHeader.NumBits;
        }

        return NumReplicatedBits;
    }

    int GetClassNameIndex() const;

    std::string& GetDescription();
    std::string& GetShortDescription();

public:
    std::string description_;
    std::string short_description_;
};

class TokenConnectionSaturated : public TokenBase {
private:
    /** Number of dropped actors */
    int NumDroppedActors;

    /** List of dropped actor name indices */
    std::vector<int> DroppedActorNameIndices;

public:
    /** Constructor, serializing members from passed in stream. */
    TokenConnectionSaturated(std::ifstream& BinaryStream) {
        std::cout<<"tellg:"<<BinaryStream.tellg()<<std::endl;
        NumDroppedActors = TokenHelper::LoadPackedInt(BinaryStream);
        std::cout<<"tellg:"<<BinaryStream.tellg()<<std::endl;

        for (int i = 0; i < NumDroppedActors; ++i) {
            std::cout<<"tellg:"<<BinaryStream.tellg()<<std::endl;
            int NameIndex = TokenHelper::LoadPackedInt(BinaryStream);
            std::cout<<"tellg:"<<BinaryStream.tellg()<<std::endl;
            DroppedActorNameIndices.push_back(NameIndex);
        }
    }
};

class TokenConnectionQueuedBits : public TokenBase {
public:
    int QueuedBits;

    TokenConnectionQueuedBits(std::ifstream& BinaryStream) {
        QueuedBits = TokenHelper::LoadPackedInt(BinaryStream);
    }
};

class TokenConnectionNetLodLevel : public TokenBase {
public:
    int NetLodLevel;

    TokenConnectionNetLodLevel(std::ifstream& BinaryStream) {
        NetLodLevel = TokenHelper::LoadPackedInt(BinaryStream);
    }
};

class TokenExportBunch : public TokenBase {
public:
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenExportBunch(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenMustBeMappedGuids : public TokenBase {
public:
    /** Number of GUIDs serialized/sent. */
    uint16_t NumGuids;
    /** Number of bits serialized/sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenMustBeMappedGuids(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&NumGuids), sizeof(NumGuids));
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenBeginContentBlock : public TokenBase {
public:
    /** Name table index of property name. */
    int ObjectNameIndex;
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenBeginContentBlock(std::ifstream& BinaryStream) {
        ObjectNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenEndContentBlock : public TokenBase {
public:
    /** Name table index of property name. */
    int ObjectNameIndex;
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenEndContentBlock(std::ifstream& BinaryStream) {
        ObjectNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenWritePropertyHandle : public TokenBase {
public:
    /** Number of bits serialized/ sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenWritePropertyHandle(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenConnectionChanged : public TokenBase {
public:
    /** Number of bits serialized/ sent. */
    int32_t AddressIndex;

    /** Constructor, serializing members from passed in stream. */
    TokenConnectionChanged(std::ifstream& BinaryStream) {
        AddressIndex = TokenHelper::LoadPackedInt(BinaryStream);
    }
};

class TokenNameReference : public TokenBase {
public:
    /** Address of connection */
    std::string Name;

    /** Constructor, serializing members from passed in stream. */
    TokenNameReference(std::ifstream& BinaryStream) {
        uint32_t Length;
        BinaryStream.read(reinterpret_cast<char*>(&Length), sizeof(Length));
        Name.resize(Length);
        BinaryStream.read(&Name[0], Length);
    }
};

class TokenConnectionReference : public TokenBase {
public:
    /** Address of connection */
    uint64_t Address;

    /** Constructor, serializing members from passed in stream. */
    TokenConnectionReference(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&Address), sizeof(Address));
    }
};

class TokenConnectionStringReference : public TokenBase {
public:
    /** Address of connection */
    std::string Address;

    /** Constructor, serializing members from passed in stream. */
    TokenConnectionStringReference(std::ifstream& BinaryStream) {
        int32_t StrLength;
        BinaryStream.read(reinterpret_cast<char*>(&StrLength), sizeof(StrLength));
        StrLength = std::abs(StrLength);
        Address.resize(StrLength);
        BinaryStream.read(&Address[0], StrLength);
    }
};

class TokenEvent : public TokenBase {
public:
    /** Name table index of event name. */
    int EventNameNameIndex;
    /** Name table index of event description. */
    int EventDescriptionNameIndex;

    /** Constructor, serializing members from passed in stream. */
    TokenEvent(std::ifstream& BinaryStream) {
        EventNameNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        EventDescriptionNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
    }
};

class TokenRawSocketData : public TokenBase {
public:
    /** Raw data. */
    std::vector<uint8_t> RawData;

    /** Constructor, serializing members from passed in stream. */
    TokenRawSocketData(std::ifstream& BinaryStream) {
        uint16_t Size;
        BinaryStream.read(reinterpret_cast<char*>(&Size), sizeof(Size));
        RawData.resize(Size);
        BinaryStream.read(reinterpret_cast<char*>(RawData.data()), Size);
    }
};

class TokenSendAck : public TokenBase {
public:
    /** Number of bits serialized/sent. */
    uint16_t NumBits;

    /** Constructor, serializing members from passed in stream. */
    TokenSendAck(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&NumBits), sizeof(NumBits));
    }
};

class TokenPropertyComparison : public TokenBase {
public:
    /** Index to the Name of the object whose properties we were comparing. */
    int ObjectNameIndex;

    /** The amount of time we spent comparing the properties. */
    float TimeSpentComparing;

    /** A BitArray describing which of the top level properties of the object were actually compared. */
    std::bitset<32> ComparedProperties;

    /** A BitArray describing which of the top level properties of the object were found to have changed. */
    std::bitset<32> ChangedProperties;

    std::vector<int> ExportedPropertyNames;

    TokenPropertyComparison() = default;

    TokenPropertyComparison(std::ifstream& BinaryStream) {
        ObjectNameIndex = TokenHelper::LoadPackedInt(BinaryStream);
        BinaryStream.read(reinterpret_cast<char*>(&TimeSpentComparing), sizeof(TimeSpentComparing));
        TokenHelper::ReadBitArray(BinaryStream, ComparedProperties);
        TokenHelper::ReadBitArray(BinaryStream, ChangedProperties);

        int NumExportedPropertyNames = TokenHelper::LoadPackedInt(BinaryStream);
        if (NumExportedPropertyNames > 0) {
            ExportedPropertyNames.resize(NumExportedPropertyNames);
            for (int i = 0; i < NumExportedPropertyNames; ++i) {
                ExportedPropertyNames[i] = TokenHelper::LoadPackedInt(BinaryStream);
            }
        }
    }
};

class TokenReplicatePropertiesMetaData : public TokenBase {
public:
    /** Index to the Name of the object whose properties we were replicating. */
    int ObjectNameIndex;

    /**
     * Whether or not we resent our entire history.
     * This is used to indicate we were resending everything for replay recording (checkpoints).
     * Note, properties that were filtered for the connection or that were inactive won't have
     * been sent, so using FilteredProperties is still required to see what was actually sent.
     */
    bool bSentAllChangedProperties;

    /**
     * A BitArray describing which of the top level properties of the object were inactive (would not
     * be replicated) during a call to ReplicateProperties.
     * The number of bits will always match the number of top level properties in the class,
     * unless bWasAnythingSent is false (in which case it will be null).
     */
    std::bitset<32> FilteredProperties;

    TokenReplicatePropertiesMetaData(std::ifstream& BinaryStream) {
        ObjectNameIndex = TokenHelper::LoadPackedInt(BinaryStream);

        uint8_t Flags;
        BinaryStream.read(reinterpret_cast<char*>(&Flags), sizeof(Flags));
        bSentAllChangedProperties = (Flags & 0x1) != 0;
        TokenHelper::ReadBitArray(BinaryStream, FilteredProperties);
    }
};

#endif //EXAMPLE_PROFILER_APP_TOKENS_H
