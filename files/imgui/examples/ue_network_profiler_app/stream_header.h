//
// Created by captainchen on 2025/1/5.
//

#ifndef EXAMPLE_PROFILER_APP_STREAM_HEADER_H
#define EXAMPLE_PROFILER_APP_STREAM_HEADER_H

#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

class StreamHeader {
public:
    static const unsigned int ExpectedMagic = 0x1DBF348C;
    static const unsigned int MinSupportedVersion = 10;

    unsigned int Magic;
    unsigned int Version;
    std::string Tag;
    std::string GameName;
    std::string URL;

    static StreamHeader& ReadHeader(std::ifstream& ParserStream) {
        StreamHeader Header;

        std::cout<<"tellg:"<<ParserStream.tellg()<<std::endl;

        //读取Magic
        int size = sizeof(Header.Magic);
        ParserStream.read(reinterpret_cast<char *>(&Header.Magic), size);
        std::cout<<"tellg:"<<ParserStream.tellg()<<std::endl;

        if (Header.Magic != StreamHeader::ExpectedMagic) {
            ParserStream.seekg(0, std::ios::beg);
            // Handle endian swap if necessary
            // For simplicity, this example assumes little-endian
            ParserStream.read(reinterpret_cast<char*>(&Header.Magic), sizeof(Header.Magic));
        }

        if (Header.Magic != StreamHeader::ExpectedMagic) {
            throw std::runtime_error("Invalid file");
        }

        ParserStream.read(reinterpret_cast<char*>(&Header.Version), sizeof(Header.Version));
        if (Header.Version < StreamHeader::MinSupportedVersion) {
            throw std::runtime_error("Invalid version");
        }

        Header.Tag = SerializeAnsiString(ParserStream);
        std::cout<<"tellg:"<<ParserStream.tellg()<<std::endl;

        Header.GameName = SerializeAnsiString(ParserStream);
        std::cout<<"tellg:"<<ParserStream.tellg()<<std::endl;

        Header.URL = SerializeAnsiString(ParserStream);
        std::cout<<"tellg:"<<ParserStream.tellg()<<std::endl;

        return Header;
    }

    StreamHeader() = default;

    StreamHeader(std::ifstream& BinaryStream) {
        BinaryStream.read(reinterpret_cast<char*>(&Magic), sizeof(Magic));
        if (Magic != ExpectedMagic) {
            return;
        }

        BinaryStream.read(reinterpret_cast<char*>(&Version), sizeof(Version));
        if (Version < MinSupportedVersion) {
            return;
        }

        Tag = SerializeAnsiString(BinaryStream);
        GameName = SerializeAnsiString(BinaryStream);
        URL = SerializeAnsiString(BinaryStream);
    }

private:
    static std::string SerializeAnsiString(std::ifstream& BinaryStream) {
        uint32_t SerializedLength;
        BinaryStream.read(reinterpret_cast<char*>(&SerializedLength), sizeof(SerializedLength));
        std::vector<char> buffer(SerializedLength);
        BinaryStream.read(buffer.data(), SerializedLength);
        return std::string(buffer.begin(), buffer.end());
    }
};
#endif //EXAMPLE_PROFILER_APP_STREAM_HEADER_H
