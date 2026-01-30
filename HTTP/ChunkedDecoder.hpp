#ifndef CHUNKEDDECODER_HPP
#define CHUNKEDDECODER_HPP

#include <string>
#include <cstddef>

struct ChunkedState
{
    enum State
    {
        CHUNK_SIZE,
        CHUNK_DATA,
        CHUNK_CRLF,
        CHUNK_DONE
    };
    State state;
    size_t chunkSize;
    bool badRequest;
    std::string body;
    ChunkedState();
    void reset();
};

class ChunkedDecoder
{
public:
    // Returns true when done (CHUNK_DONE or badRequest); false when more data is needed.
    static bool parse(std::string &in, ChunkedState &st);
};

#endif
