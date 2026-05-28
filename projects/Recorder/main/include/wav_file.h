#pragma once

#include <cstdint>
#include <cstdio>

// Write a placeholder WAV header. Call wav_file_finalize() after writing PCM data.
bool wav_file_write_header(FILE* file, uint16_t channels, uint32_t sampleRate, uint32_t dataSize);

// Finalize WAV header by updating chunk sizes. file must be open for read/write.
bool wav_file_finalize(FILE* file);

// Read WAV header and return parameters. File pointer is left at start of PCM data.
bool wav_file_read_header(FILE* file, uint16_t& channels, uint32_t& sampleRate, uint32_t& dataSize);
