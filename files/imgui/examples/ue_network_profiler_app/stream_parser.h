//
// Created by captainchen on 2025/1/5.
//

#ifndef EXAMPLE_PROFILER_APP_STREAM_PARSER_H
#define EXAMPLE_PROFILER_APP_STREAM_PARSER_H


#include "network_stream.h"
#include "tokens.h"

class StreamParser {
public:
    /**
     * Helper function for handling housekeeping that needs to happen when we parse a new actor
     * We used to emit actors before properties, but now we emit properties before actors
     * So when we are about to parse a new actor, we need to copy the properties up to that point to this new actor
     *
     * @param	token_replicate_actor		Actor token
     * @param	last_properties			Properties to be copied to the actor
     */
    static void FinishActorProperties(TokenReplicateActor& token_replicate_actor, std::vector<TokenReplicateProperty>& last_properties, std::vector<TokenWritePropertyHeader>& last_property_headers)
    {
        for (size_t i = 0; i < last_properties.size(); i++)
        {
            token_replicate_actor.Properties.push_back(last_properties[i]);
        }
        last_properties.clear();

        for (size_t i = 0; i < last_property_headers.size(); i++)
        {
            token_replicate_actor.PropertyHeaders.push_back(last_property_headers[i]);
        }
        last_property_headers.clear();
    }

    static NetworkStream Parse(std::istream& parserStream) {
        std::cout<<"tellg:"<<parserStream.tellg()<<std::endl;

        auto startTime = std::time(nullptr);

        NetworkStream networkStream;

        std::istream& parser_stream_return = StreamHeader::ReadHeader(parserStream);

        std::cout<<"tellg:"<<parserStream.tellg()<<std::endl;

        std::vector<TokenBase*> currentFrameTokens;
        TokenReplicateActor* lastActorToken = nullptr;
        std::vector<TokenReplicateProperty> lastProperties;
        std::vector<TokenWritePropertyHeader> lastPropertyHeaders;

        std::map<int, TokenPropertyComparison> objectNamesToPropertyComparisons;

        TokenFrameMarker* lastFrameMarker = nullptr;

        int count = 0;

        bool hasReachedEndOfStream = false;

        std::vector<TokenBase*> tokenList;

        float frameStartTime = -1.0f;
        float frameEndTime = -1.0f;

        while (!hasReachedEndOfStream) {
            //如果已经读取到文件末尾，就不再读取
            if (parserStream.eof()) {
                break;
            }

            TokenBase* token = nullptr;

            try {
                token = TokenBase::ReadNextToken(parserStream, networkStream);
            } catch (std::ios::failure&) {
                break;
            }

            if (token->GetTokenType() == ETokenTypes::NameReference) {
                networkStream.NameArray.push_back(static_cast<TokenNameReference*>(token)->Name);

                if (networkStream.NameArray.back() == "Unreal") {
                    networkStream.NameIndexUnreal = networkStream.NameArray.size() - 1;
                }
                continue;
            }

            if (token->GetTokenType() == ETokenTypes::ConnectionReference) {
                if (networkStream.GetVersion() < 12) {
                    networkStream.AddressArray.push_back(static_cast<TokenConnectionReference*>(token)->Address);
                } else {
                    networkStream.StringAddressArray.push_back(static_cast<TokenConnectionStringReference*>(token)->Address);
                }
                continue;
            }

            if (token->GetTokenType() == ETokenTypes::ConnectionChange) {
                networkStream.CurrentConnectionIndex = static_cast<TokenConnectionChanged*>(token)->AddressIndex;
                continue;
            }

            tokenList.push_back(token);

            if (token->GetTokenType() == ETokenTypes::FrameMarker) {
                auto tokenFrameMarker = static_cast<TokenFrameMarker*>(token);

                if (frameStartTime < 0) {
                    frameStartTime = tokenFrameMarker->RelativeTime;
                    frameEndTime = tokenFrameMarker->RelativeTime;
                } else {
                    frameEndTime = tokenFrameMarker->RelativeTime;
                }
            }
        }

        for (size_t i = 0; i < tokenList.size(); i++) {

            TokenBase* token = tokenList[i];

            if (((token->GetTokenType() == ETokenTypes::FrameMarker) || (token->GetTokenType() == ETokenTypes::EndOfStreamMarker)) && (currentFrameTokens.size() > 0)) {
                float deltaTime = 1 / 30.0f;
                if (token->GetTokenType() == ETokenTypes::FrameMarker && lastFrameMarker != nullptr) {
                    deltaTime = static_cast<TokenFrameMarker*>(token)->RelativeTime - lastFrameMarker->RelativeTime;
                }

                currentFrameTokens.clear();

                lastActorToken = nullptr;
            }

            if (token->GetTokenType() == ETokenTypes::FrameMarker) {
                lastFrameMarker = static_cast<TokenFrameMarker*>(token);
                objectNamesToPropertyComparisons.clear();
            }

            if (token->GetTokenType() == ETokenTypes::EndOfStreamMarker) {
                hasReachedEndOfStream = true;
            } else {
                if (token->GetTokenType() == ETokenTypes::ReplicateActor) {
                    FinishActorProperties(*static_cast<TokenReplicateActor*>(token), lastProperties, lastPropertyHeaders);
                    lastActorToken = static_cast<TokenReplicateActor*>(token);
                } else if (token->GetTokenType() == ETokenTypes::SendRPC) {
                    auto tokenSendRPC = static_cast<TokenSendRPC*>(token);
                }

                if (token->GetTokenType() == ETokenTypes::ReplicateProperty) {
                    auto tokenReplicateProperty = static_cast<TokenReplicateProperty*>(token);
                    lastProperties.push_back(*tokenReplicateProperty);
                } else if (token->GetTokenType() == ETokenTypes::WritePropertyHeader) {
                    auto tokenWritePropertyHeader = static_cast<TokenWritePropertyHeader*>(token);
                    lastPropertyHeaders.push_back(*tokenWritePropertyHeader);
                } else if (token->GetTokenType() == ETokenTypes::PropertyComparison) {
                    TokenPropertyComparison* tokenPropertyComparison = static_cast<TokenPropertyComparison*>(token);
                    objectNamesToPropertyComparisons[tokenPropertyComparison->ObjectNameIndex] = *tokenPropertyComparison;
                } else if (token->GetTokenType() == ETokenTypes::ReplicatePropertiesMetaData) {
                    auto tokenReplicatePropertiesMetaData = static_cast<TokenReplicatePropertiesMetaData*>(token);
                    TokenPropertyComparison* comparison = nullptr;
                    if (objectNamesToPropertyComparisons.find(tokenReplicatePropertiesMetaData->ObjectNameIndex) != objectNamesToPropertyComparisons.end()) {
                        comparison = &objectNamesToPropertyComparisons[tokenReplicatePropertiesMetaData->ObjectNameIndex];
                    } else {
                        std::cout << networkStream.GetName(tokenReplicatePropertiesMetaData->ObjectNameIndex) << std::endl;
                    }
                } else {
                    currentFrameTokens.push_back(token);
                }
            }
        }

        double parseTime = std::difftime(std::time(nullptr), startTime);
        std::cout << "Parsing " << parserStream.tellg() / 1024 / 1024 << " MBytes in stream took " << parseTime << " seconds" << std::endl;

        return networkStream;
    }
public:
    static NetworkStream* network_stream_;
};




#endif //EXAMPLE_PROFILER_APP_STREAM_PARSER_H
