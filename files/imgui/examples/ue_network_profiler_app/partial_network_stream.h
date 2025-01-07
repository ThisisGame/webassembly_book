//
// Created by captainchen on 2025/1/6.
//

#ifndef EXAMPLE_PROFILER_APP_PARTIAL_NETWORK_STREAM_H
#define EXAMPLE_PROFILER_APP_PARTIAL_NETWORK_STREAM_H

#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <bitset>
#include <unordered_set>
#include "tokens.h"
#include "main_window.h"
#include "tree_node_collection.h"
#include "tree_node.h"

template <typename T>
class UniqueItem
{
public:
    int Count = 0; // Number of non unique items
    T* FirstItem = nullptr; // Current instance

    virtual void OnItemAdded(T* Item)
    {
        if (FirstItem == nullptr)
        {
            FirstItem = Item;
        }
        Count++;
    }
};

template <typename UniqueT, typename ItemT>
class UniqueItemTracker
{
public:
    std::unordered_map<int, UniqueT> UniqueItems;

    void AddItem(ItemT* Item, int Key)
    {
        if (UniqueItems.find(Key) == UniqueItems.end())
        {
            UniqueItems[Key] = UniqueT();
        }

        UniqueItems[Key].OnItemAdded(Item);
    }
};

class UniqueProperty : public UniqueItem<TokenReplicateProperty>
{
public:
    long SizeBits = 0;

    void OnItemAdded(TokenReplicateProperty* Item) override
    {
        UniqueItem::OnItemAdded(Item);
        SizeBits += Item->NumBits;
    }
};


class UniquePropertyHeader : public UniqueItem<TokenWritePropertyHeader>
{
public:
    long SizeBits = 0;

    void OnItemAdded(TokenWritePropertyHeader* Item) override
    {
        UniqueItem::OnItemAdded(Item);
        SizeBits += Item->NumBits;
    }
};

class UniqueActor : public UniqueItem<TokenReplicateActor>
{
public:
    long SizeBits = 0;
    float TimeInMS = 0;
    int ReplicatedCount = 0;

    UniqueItemTracker<UniqueProperty, TokenReplicateProperty> Properties;

    void OnItemAdded(TokenReplicateActor* Item) override
    {
        UniqueItem::OnItemAdded(Item);

        if (!Item->Properties.empty())
        {
            ReplicatedCount++;
        }

        TimeInMS += Item->TimeInMS;

        // Iterate through all properties, count the number and size of properties
        for (auto& Property : Item->Properties)
        {
            SizeBits += Property.NumBits;
            Properties.AddItem(&Property, Property.PropertyNameIndex);
        }

        for (auto& PropertyHeader : Item->PropertyHeaders)
        {
            SizeBits += PropertyHeader.NumBits;
        }
    }
};

class PartialNetworkStream
{
public:
    // Time/ frames
    float StartTime = -1; // Normalized start time of partial stream.
    float EndTime = 0; // Normalized end time of partial stream.
    int NumFrames = 0; // Number of frames this summary covers.
    int NumEvents = 0; // Number of events in this frame.

    // Actor/ Property replication
    int ActorCount = 0; // Number of actors in this partial stream.
    int ReplicatedActorCount = 0; // Number of replicated actors in this partial stream.
    float ActorReplicateTimeInMS = 0.0f; // Total actor replication time in ms.
    int PropertyCount = 0; // Number of properties replicated.
    int ReplicatedSizeBits = 0; // Total size of properties replicated.

    // RPC
    int RPCCount = 0; // Number of RPCs replicated.
    int RPCSizeBits = 0; // Total size of RPC replication.

    // SendBunch
    int SendBunchCount = 0; // Number of times SendBunch was called.
    int SendBunchSizeBits = 0; // Total size of bytes sent.
    int SendBunchHeaderSizeBits = 0; // Total size of bunch headers sent.
    std::unordered_map<int, int> SendBunchCountPerChannel; // Call count per channel type.
    std::unordered_map<int, int> SendBunchSizeBitsPerChannel; // Size per channel type.
    std::unordered_map<int, int> SendBunchHeaderSizeBitsPerChannel; // Size of bunch headers per channel type.
    std::unordered_map<int, int> SendBunchPayloadSizeBitsPerChannel; // Size of bunch payloads per channel type.

    // Low level socket
    int UnrealSocketCount = 0; // Number of low level socket sends on "Unreal" socket type.
    int UnrealSocketSize = 0; // Total size of bytes sent on "Unreal" socket type.
    int OtherSocketCount = 0; // Number of low level socket sends on non-"Unreal" socket types.
    int OtherSocketSize = 0; // Total size of bytes sent on non-"Unreal" socket type.

    // Acks
    int SendAckCount = 0; // Number of acks sent.
    int SendAckSizeBits = 0; // Total size of acks sent.

    // Exported GUIDs
    int ExportBunchCount = 0; // Number of GUID export bunches sent.
    int ExportBunchSizeBits = 0; // Total size of exported GUIDs sent.

    // Must be mapped GUIDs
    int MustBeMappedGuidCount = 0; // Number of must be mapped GUIDs.
    int MustBeMappedGuidSizeBits = 0; // Total size of exported GUIDs sent.

    // Content blocks
    int ContentBlockHeaderCount = 0; // Number of content block headers.
    int ContentBlockHeaderSizeBits = 0; // Total size of content block headers.
    int ContentBlockFooterCount = 0; // Number of content block footers.
    int ContentBlockFooterSizeBits = 0; // Total size of content block footers.

    // Property handles
    int PropertyHandleCount = 0; // Number of property handles.
    int PropertyHandleSizeBits = 0; // Total size of property handles.

    // Detailed information.
    std::vector<TokenBase*> Tokens; // List of all tokens in this substream.
    UniqueItemTracker<UniqueActor, TokenReplicateActor> UniqueActors; // List of all all unique actors in this substream.

    // Cached internal data.
    float FirstFrameDeltaTime = 0; // Delta time of first frame. Passed to constructor as we can't calculate it.
    int NameIndexUnreal = -1; // Index of "Unreal" name in network stream name table.

    PartialNetworkStream(std::vector<TokenBase*> InTokens, int InNameIndexUnreal, float InDeltaTime)
    {
        NameIndexUnreal = InNameIndexUnreal;
        FirstFrameDeltaTime = InDeltaTime;

        for (auto Token : InTokens)
        {
            Tokens.push_back(Token);
        }

        CreateSummary(NameIndexUnreal, FirstFrameDeltaTime, FilterValues());
    }

    PartialNetworkStream(PartialNetworkStream* InStream, FilterValues InFilterValues)
    {
        NameIndexUnreal = InStream->NameIndexUnreal;
        FirstFrameDeltaTime = InStream->FirstFrameDeltaTime;

        for (auto Token : InStream->Tokens)
        {
            if (Token->MatchesFilters(InFilterValues))
            {
                Tokens.push_back(Token);
            }
        }

        CreateSummary(NameIndexUnreal, FirstFrameDeltaTime, InFilterValues);
    }

    PartialNetworkStream(MainWindow* InMainWindow, std::vector<PartialNetworkStream*> InStreams, int InStartFrame, int InEndFrame, int InNameIndexUnreal, FilterValues InFilterValues, float InDeltaTime)
    {
        NameIndexUnreal = InNameIndexUnreal;
        FirstFrameDeltaTime = InDeltaTime;

        InMainWindow->ShowProgress(true);
        InMainWindow->UpdateProgress(0);

        for (int i = InStartFrame; i < InEndFrame; i++)
        {
            PartialNetworkStream* Stream = InStreams[i];

            if (i % 10 == 0)
            {
                float Percent = static_cast<float>(i - InStartFrame) / static_cast<float>(InEndFrame - InStartFrame);
                InMainWindow->UpdateProgress(static_cast<int>(Percent * 100));
            }

            for (auto Token : Stream->Tokens)
            {
                if (Token->MatchesFilters(InFilterValues))
                {
                    Tokens.push_back(Token);
                    UpdateSummary(Token, InFilterValues);
                }
            }
        }

        InMainWindow->ShowProgress(false);

        EndTime += InDeltaTime;
    }

    PartialNetworkStream(int InNameIndexUnreal, float InDeltaTime)
    {
        NameIndexUnreal = InNameIndexUnreal;
        FirstFrameDeltaTime = InDeltaTime;
    }

    void AddStream(PartialNetworkStream* InStream)
    {
        for (auto Token : InStream->Tokens)
        {
            Tokens.push_back(Token);
            UpdateSummary(Token, FilterValues());
        }
    }

    PartialNetworkStream* Filter(FilterValues InFilterValues)
    {
        return new PartialNetworkStream(this, InFilterValues);
    }

protected:
    void UpdateSummary(TokenBase* Token, FilterValues InFilterValues)
    {
        switch (Token->TokenType)
        {
            case ETokenTypes::FrameMarker:
            {
                auto TokenFrameMarker = dynamic_cast<class TokenFrameMarker*>(Token);
                if (StartTime < 0)
                {
                    StartTime = TokenFrameMarker->RelativeTime;
                    EndTime = TokenFrameMarker->RelativeTime;
                }
                else
                {
                    EndTime = TokenFrameMarker->RelativeTime;
                }
                NumFrames++;
                break;
            }
            case ETokenTypes::SocketSendTo:
            {
                auto TokenSocketSendTo = static_cast<class TokenSocketSendTo*>(Token);
                if (TokenSocketSendTo->SocketNameIndex == NameIndexUnreal)
                {
                    UnrealSocketCount++;
                    UnrealSocketSize += TokenSocketSendTo->BytesSent;
                }
                else
                {
                    OtherSocketCount++;
                    OtherSocketSize += TokenSocketSendTo->BytesSent;
                }
                break;
            }
            case ETokenTypes::SendBunch:
            {
                auto TokenSendBunch = static_cast<class TokenSendBunch*>(Token);
                SendBunchCount++;
                SendBunchSizeBits += TokenSendBunch->GetNumTotalBits();
                SendBunchHeaderSizeBits += TokenSendBunch->NumHeaderBits;

                int ChannelTypeIndex = TokenSendBunch->GetChannelTypeIndex();

                if (SendBunchCountPerChannel.find(ChannelTypeIndex) != SendBunchCountPerChannel.end())
                {
                    SendBunchCountPerChannel[ChannelTypeIndex]++;
                    SendBunchSizeBitsPerChannel[ChannelTypeIndex] += TokenSendBunch->GetNumTotalBits();
                    SendBunchHeaderSizeBitsPerChannel[ChannelTypeIndex] += TokenSendBunch->NumHeaderBits;
                    SendBunchPayloadSizeBitsPerChannel[ChannelTypeIndex] += TokenSendBunch->NumPayloadBits;
                }
                else
                {
                    SendBunchCountPerChannel[ChannelTypeIndex] = 1;
                    SendBunchSizeBitsPerChannel[ChannelTypeIndex] = TokenSendBunch->GetNumTotalBits();
                    SendBunchHeaderSizeBitsPerChannel[ChannelTypeIndex] = TokenSendBunch->NumHeaderBits;
                    SendBunchPayloadSizeBitsPerChannel[ChannelTypeIndex] = TokenSendBunch->NumPayloadBits;
                }

                break;
            }
            case ETokenTypes::SendRPC:
            {
                auto TokenSendRPC = static_cast<class TokenSendRPC*>(Token);
                RPCCount++;
                RPCSizeBits += TokenSendRPC->GetNumTotalBits();
                break;
            }
            case ETokenTypes::ReplicateActor:
            {
                auto TokenReplicateActor = static_cast<class TokenReplicateActor*>(Token);
                ActorCount++;

                if (TokenReplicateActor->Properties.size() > 0)
                {
                    ReplicatedActorCount++;
                }

                ActorReplicateTimeInMS += TokenReplicateActor->TimeInMS;

                for (auto Property : TokenReplicateActor->Properties)
                {
                    if (Property.MatchesFilters(InFilterValues))
                    {
                        PropertyCount++;
                        ReplicatedSizeBits += Property.NumBits;
                    }
                }

                for (auto PropertyHeader : TokenReplicateActor->PropertyHeaders)
                {
                    if (PropertyHeader.MatchesFilters(InFilterValues))
                    {
                        ReplicatedSizeBits += PropertyHeader.NumBits;
                    }
                }

                UniqueActors.AddItem(TokenReplicateActor, TokenReplicateActor->GetClassNameIndex());

                break;
            }
            case ETokenTypes::Event:
                NumEvents++;
                break;
            case ETokenTypes::RawSocketData:
                break;
            case ETokenTypes::SendAck:
            {
                auto TokenSendAck = static_cast<class TokenSendAck*>(Token);
                SendAckCount++;
                SendAckSizeBits += TokenSendAck->NumBits;
                break;
            }
            case ETokenTypes::ExportBunch:
            {
                auto TokenExportBunch = static_cast<class TokenExportBunch*>(Token);
                ExportBunchCount++;
                ExportBunchSizeBits += TokenExportBunch->NumBits;
                break;
            }
            case ETokenTypes::MustBeMappedGuids:
            {
                auto TokenMustBeMappedGuids = static_cast<class TokenMustBeMappedGuids*>(Token);
                MustBeMappedGuidCount += TokenMustBeMappedGuids->NumGuids;
                MustBeMappedGuidSizeBits += TokenMustBeMappedGuids->NumBits;
                break;
            }
            case ETokenTypes::BeginContentBlock:
            {
                auto TokenBeginContentBlock = static_cast<class TokenBeginContentBlock*>(Token);
                ContentBlockHeaderCount++;
                ContentBlockHeaderSizeBits += TokenBeginContentBlock->NumBits;
                break;
            }
            case ETokenTypes::EndContentBlock:
            {
                auto TokenEndContentBlock = static_cast<class TokenEndContentBlock*>(Token);
                ContentBlockFooterCount++;
                ContentBlockFooterSizeBits += TokenEndContentBlock->NumBits;
                break;
            }
            case ETokenTypes::WritePropertyHandle:
            {
                auto TokenWritePropertyHandle = static_cast<class TokenWritePropertyHandle*>(Token);
                PropertyHandleCount++;
                PropertyHandleSizeBits += TokenWritePropertyHandle->NumBits;
                break;
            }
            case ETokenTypes::ConnectionSaturated:
            {
                auto TokenConnectionSaturated = static_cast<class TokenConnectionSaturated*>(Token);
                break;
            }
            case ETokenTypes::ConnectionQueuedBits:
            {
                auto TokenConnectionQueuedBits = static_cast<class TokenConnectionQueuedBits*>(Token);
                break;
            }
            case ETokenTypes::ConnectionNetLodLevel:
            {
                auto TokenConnectionNetLodLevel = static_cast<class TokenConnectionNetLodLevel*>(Token);
                break;
            }
            default:
                throw std::invalid_argument("Invalid token type");
        }
    }

    void CreateSummary(int NameIndexUnreal, float DeltaTime, FilterValues InFilterValues)
    {
        for (auto Token : Tokens)
        {
            UpdateSummary(Token, InFilterValues);
        }

        EndTime += DeltaTime;
    }

    void ToDetailedTreeView(TreeNodeCollection& Nodes, FilterValues InFilterValues)
    {
        Nodes.Clear();

        for (auto Token : Tokens)
        {
            Token->ToDetailedTreeView(Nodes, InFilterValues);
        }
    }

private:
    TreeNode* AddNode(TreeNodeCollection& Nodes, int Index, const std::string& Value)
    {
        if (Nodes.size() <= Index)
        {
            return Nodes.Add(Value);
        }

        Nodes[Index].Text = Value;

        return &Nodes[Index];
    }
};


#endif //EXAMPLE_PROFILER_APP_PARTIAL_NETWORK_STREAM_H
