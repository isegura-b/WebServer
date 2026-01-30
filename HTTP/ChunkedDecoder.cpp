#include "ChunkedDecoder.hpp"
#include <cstdlib>

ChunkedState::ChunkedState()
    : state(CHUNK_SIZE), chunkSize(0), badRequest(false)
{
}

void ChunkedState::reset()
{
    state = CHUNK_SIZE;
    chunkSize = 0;
    badRequest = false;
    body.clear();
}

bool ChunkedDecoder::parse(std::string &in, ChunkedState &st)
{
    while (true)
    {
        if (st.state == ChunkedState::CHUNK_SIZE)
        {
            size_t pos = in.find("\r\n");
            if (pos == std::string::npos)
                return false;
            std::string line = in.substr(0, pos);
            in.erase(0, pos + 2);

            size_t sc = line.find(';');
            if (sc != std::string::npos)
                line = line.substr(0, sc);

            char *endptr = NULL;
            long sz = std::strtol(line.c_str(), &endptr, 16);
            if (endptr == line.c_str() || sz < 0)
            {
                st.badRequest = true;
                st.state = ChunkedState::CHUNK_DONE;
                return true;
            }
            if (sz == 0)
            {
                st.state = ChunkedState::CHUNK_DONE;
                continue;
            }
            st.chunkSize = static_cast<size_t>(sz);
            st.state = ChunkedState::CHUNK_DATA;
        }
        if (st.state == ChunkedState::CHUNK_DATA)
        {
            if (in.size() < st.chunkSize)
                return false;
            st.body.append(in, 0, st.chunkSize);
            in.erase(0, st.chunkSize);
            st.state = ChunkedState::CHUNK_CRLF;
        }
        if (st.state == ChunkedState::CHUNK_CRLF)
        {
            if (in.size() < 2)
                return false;
            if (in.substr(0, 2) != "\r\n")
            {
                st.badRequest = true;
                st.state = ChunkedState::CHUNK_DONE;
                return true;
            }
            in.erase(0, 2);
            st.state = ChunkedState::CHUNK_SIZE;
        }
        if (st.state == ChunkedState::CHUNK_DONE)
        {
            size_t end = in.find("\r\n\r\n");
            if (end != std::string::npos)
            {
                in.erase(0, end + 4);
                return true;
            }
            if (in.size() >= 2 && in.substr(0, 2) == "\r\n")
            {
                in.erase(0, 2);
                return true;
            }
            return false;
        }
    }
    return false;
}
