#include "wav_file.h"
#include <cstring>

struct WavHeader {
    char     riff[4];
    uint32_t fileSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     data[4];
    uint32_t dataSize;
};

static_assert(sizeof(WavHeader) == 44, "WAV header must be 44 bytes");

bool wav_file_write_header(FILE* file, uint16_t channels, uint32_t sampleRate, uint32_t dataSize)
{
    WavHeader h = {};
    h.riff[0] = 'R'; h.riff[1] = 'I'; h.riff[2] = 'F'; h.riff[3] = 'F';
    h.fileSize = dataSize + 36;
    h.wave[0] = 'W'; h.wave[1] = 'A'; h.wave[2] = 'V'; h.wave[3] = 'E';
    h.fmt[0]  = 'f'; h.fmt[1]  = 'm'; h.fmt[2]  = 't'; h.fmt[3]  = ' ';
    h.fmtSize = 16;
    h.audioFormat = 1; // PCM
    h.channels = channels;
    h.sampleRate = sampleRate;
    h.bitsPerSample = 16;
    h.blockAlign = channels * (h.bitsPerSample / 8);
    h.byteRate = sampleRate * h.blockAlign;
    h.data[0] = 'd'; h.data[1] = 'a'; h.data[2] = 't'; h.data[3] = 'a';
    h.dataSize = dataSize;

    return fwrite(&h, sizeof(h), 1, file) == 1;
}

bool wav_file_finalize(FILE* file)
{
    long fileSize = ftell(file);
    if (fileSize < static_cast<long>(sizeof(WavHeader))) return false;

    uint32_t dataSize = static_cast<uint32_t>(fileSize - sizeof(WavHeader));
    uint32_t riffSize = static_cast<uint32_t>(fileSize - 8);

    if (fseek(file, 4, SEEK_SET) != 0) return false;
    if (fwrite(&riffSize, 4, 1, file) != 1) return false;

    if (fseek(file, 40, SEEK_SET) != 0) return false;
    if (fwrite(&dataSize, 4, 1, file) != 1) return false;

    fseek(file, 0, SEEK_END);
    return true;
}

bool wav_file_read_header(FILE* file, uint16_t& channels, uint32_t& sampleRate, uint32_t& dataSize)
{
    WavHeader h;
    if (fread(&h, sizeof(h), 1, file) != 1) return false;
    if (std::memcmp(h.riff, "RIFF", 4) != 0 || std::memcmp(h.wave, "WAVE", 4) != 0) return false;
    if (h.audioFormat != 1) return false; // PCM only
    if (h.bitsPerSample != 16) return false; // s16 only

    channels = h.channels;
    sampleRate = h.sampleRate;
    dataSize = h.dataSize;
    return true;
}
