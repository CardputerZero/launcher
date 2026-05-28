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
    // RIFF header
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    if (fread(riff, 1, 4, file) != 4) return false;
    if (fread(&fileSize, 4, 1, file) != 1) return false;
    if (fread(wave, 1, 4, file) != 4) return false;
    if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) return false;

    channels = 0;
    sampleRate = 0;
    dataSize = 0;
    uint16_t bitsPerSample = 0;

    while (!feof(file)) {
        char chunkId[4];
        uint32_t chunkSize;
        if (fread(chunkId, 1, 4, file) != 4) break;
        if (fread(&chunkSize, 4, 1, file) != 1) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat;
            if (fread(&audioFormat, 2, 1, file) != 1) return false;
            if (audioFormat != 1) return false; // PCM only
            if (fread(&channels, 2, 1, file) != 1) return false;
            if (fread(&sampleRate, 4, 1, file) != 1) return false;
            // Skip byteRate (4) + blockAlign (2)
            if (fseek(file, 6, SEEK_CUR) != 0) return false;
            if (fread(&bitsPerSample, 2, 1, file) != 1) return false;
            // Skip any extra fmt bytes (pad to even boundary)
            uint32_t skip = chunkSize > 16 ? chunkSize - 16 : 0;
            if (skip > 0) {
                if (fseek(file, skip, SEEK_CUR) != 0) return false;
            }
        }
        else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataSize = chunkSize;
            // File pointer is now at start of PCM data
            return (channels != 0 && sampleRate != 0 && dataSize > 0 && bitsPerSample == 16);
        }
        else {
            // Skip unknown chunk (pad to even size as per RIFF spec)
            uint32_t skip = (chunkSize + 1) & ~1U;
            if (fseek(file, skip, SEEK_CUR) != 0) return false;
        }
    }

    return false;
}
