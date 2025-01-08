//
// Created by captainchen on 2025/1/8.
//

#ifndef EXAMPLE_PROFILER_APP_BUFFER_STREAM_H
#define EXAMPLE_PROFILER_APP_BUFFER_STREAM_H

#include <iostream>
#include <fstream>

class BufferStream {
public:
    //将std::ifstream转换为BufferStream
    BufferStream(std::ifstream& parserStream) {
        parserStream.seekg(0, std::ios::end);
        size_t size = parserStream.tellg();
        parserStream.seekg(0, std::ios::beg);

        buffer_ = new char[size];
        parserStream.read(buffer_, size);
        size_ = size;
        position_ = 0;
    }

    BufferStream(char* buffer, size_t size)
            : buffer_(buffer), size_(size), position_(0) {}

    void read(char* output, size_t size) {
        if (position_ + size > size_) {
            throw std::runtime_error("Read beyond buffer size");
        }
        std::memcpy(output, buffer_ + position_, size);
        position_ += size;
    }

    bool eof() const {
        return position_ >= size_;
    }

    size_t tellg() const {
        return position_;
    }

    void seekg(size_t position, std::ios_base::seekdir dir) {
        if (position > size_) {
            throw std::runtime_error("Seek beyond buffer size");
        }
        position_ = position;
    }

    char get() {
        if (position_ >= size_) {
            throw std::runtime_error("Read beyond buffer size");
        }
        return buffer_[position_++];
    }

private:
    char* buffer_;
    size_t size_;
    size_t position_;
};


#endif //EXAMPLE_PROFILER_APP_BUFFER_STREAM_H
