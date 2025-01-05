//
// Created by captainchen on 2025/1/5.
//
#include "tokens.h"
#include "stream_parser.h"

TokenBase* TokenBase::ReadNextToken(std::istream& BinaryStream, NetworkStream& InNetworkStream) {
    TokenBase* SerializedToken = nullptr;
    ETokenTypes TokenType;

    char TokenChar;
    BinaryStream.read(&TokenChar, 1);

    // Token type is stored in the first byte of the token
    TokenType = static_cast<ETokenTypes>(TokenChar);

    switch (TokenType) {
        case ETokenTypes::FrameMarker:
            SerializedToken = new TokenFrameMarker(BinaryStream);
            break;
        case ETokenTypes::SocketSendTo:
            SerializedToken = new TokenSocketSendTo(BinaryStream);
            break;
        case ETokenTypes::SendBunch:
            SerializedToken = new TokenSendBunch(BinaryStream, InNetworkStream.GetVersion());
            break;
        case ETokenTypes::SendRPC:
            SerializedToken = new TokenSendRPC(BinaryStream, InNetworkStream.GetVersion());
            break;
        case ETokenTypes::ReplicateActor:
            SerializedToken = new TokenReplicateActor(BinaryStream);
            break;
        case ETokenTypes::ReplicateProperty:
            SerializedToken = new TokenReplicateProperty(BinaryStream);
            break;
        case ETokenTypes::EndOfStreamMarker:
            SerializedToken = new TokenEndOfStreamMarker();
            break;
        case ETokenTypes::Event:
            SerializedToken = new TokenEvent(BinaryStream);
            break;
        case ETokenTypes::RawSocketData:
            SerializedToken = new TokenRawSocketData(BinaryStream);
            break;
        case ETokenTypes::SendAck:
            SerializedToken = new TokenSendAck(BinaryStream);
            break;
        case ETokenTypes::WritePropertyHeader:
            SerializedToken = new TokenWritePropertyHeader(BinaryStream);
            break;
        case ETokenTypes::ExportBunch:
            SerializedToken = new TokenExportBunch(BinaryStream);
            break;
        case ETokenTypes::MustBeMappedGuids:
            SerializedToken = new TokenMustBeMappedGuids(BinaryStream);
            break;
        case ETokenTypes::BeginContentBlock:
            SerializedToken = new TokenBeginContentBlock(BinaryStream);
            break;
        case ETokenTypes::EndContentBlock:
            SerializedToken = new TokenEndContentBlock(BinaryStream);
            break;
        case ETokenTypes::WritePropertyHandle:
            SerializedToken = new TokenWritePropertyHandle(BinaryStream);
            break;
        case ETokenTypes::NameReference:
            SerializedToken = new TokenNameReference(BinaryStream);
            break;
        case ETokenTypes::ConnectionReference:
            if (InNetworkStream.GetVersion() < 12) {
                SerializedToken = new TokenConnectionReference(BinaryStream);
            } else {
                SerializedToken = new TokenConnectionStringReference(BinaryStream);
            }
            break;
        case ETokenTypes::ConnectionChange:
            SerializedToken = new TokenConnectionChanged(BinaryStream);
            break;
        case ETokenTypes::PropertyComparison:
            SerializedToken = new TokenPropertyComparison(BinaryStream);
            break;
        case ETokenTypes::ReplicatePropertiesMetaData:
            SerializedToken = new TokenReplicatePropertiesMetaData(BinaryStream);
            break;
        case ETokenTypes::ConnectionSaturated:
            SerializedToken = new TokenConnectionSaturated(BinaryStream);
            break;
        default:
            throw std::invalid_argument("Invalid token type");
    }

    SerializedToken->TokenType = TokenType;
    SerializedToken->ConnectionIndex = InNetworkStream.CurrentConnectionIndex;
    return SerializedToken;
}

int TokenReplicateActor::GetClassNameIndex() const {
    return StreamParser::network_stream_->GetClassNameIndex(ActorNameIndex);
}