#ifndef __AVI_H__
#define __AVI_H__

#include <Arduino.h>
#include <FS.h>

class aviwriter {
private:
    File aviFile;
    uint32_t totalFrames = 0;
    uint32_t moviSize = 0;
    uint32_t fps = 30;
    uint32_t width = 0;
    uint32_t height = 0;
    
    // Allocate a vector or fixed buffer for frame sizes to build the index chunk at the end
    // For ESP32-S3, PSRAM is highly recommended if you have thousands of frames.
    uint32_t* frameSizes = nullptr;
    uint32_t maxFrames = 0;

    void write32(uint32_t val) {
        aviFile.write((uint8_t*)&val, 4);
    }

    void write16(uint16_t val) {
        aviFile.write((uint8_t*)&val, 2);
    }

public:
    aviwriter() {}
    
    ~aviwriter() {
        if (frameSizes) free(frameSizes);
    }

    bool begin(File file, uint32_t w, uint32_t h, uint32_t frameRate, uint32_t expectedMaxFrames) {
        aviFile = file;
        width = w;
        height = h;
        fps = frameRate;
        maxFrames = expectedMaxFrames;
        totalFrames = 0;
        moviSize = 0;

        // Allocate memory for frame tracking (Index creation)
        frameSizes = (uint32_t*)malloc(maxFrames * sizeof(uint32_t));
        if (!frameSizes) return false;

        if (!aviFile) return false;

        // Placeholder for Header: 204 bytes total
        uint8_t dummy[204] = {0};
        aviFile.write(dummy, 204);
        
        return true;
    }

    bool addFrame(uint8_t* jpegBuffer, uint32_t jpegLen) {
        if (totalFrames >= maxFrames)return false;

        // 1. Write Chunk ID for video frame ('00dc' means Video Chunk)
        aviFile.write((const uint8_t*)"00dc", 4);
        
        // 2. Pad to even length per RIFF specification
        uint32_t paddedLen = (jpegLen + 1) & ~1;
        write32(paddedLen);

        // 3. Write raw JPEG data
        aviFile.write(jpegBuffer, jpegLen);
        
        // 4. Handle padding byte if odd
        if (jpegLen % 2 != 0) {
            aviFile.write((uint8_t)0);
        }

        // Store standard length (excluding padding) for the final index table
        frameSizes[totalFrames] = jpegLen;
        
        // Accumulate total movi chunk sizes (chunk id + size field + padded length)
        moviSize += 8 + paddedLen;
        totalFrames++;
        
        return true;
    }

    void close() {
        if (!aviFile) return;

        // 1. Write the idx1 chunk (Index table) right after the video chunks
        uint32_t idxStartPos = aviFile.position();
        aviFile.write((const uint8_t*)"idx1", 4);
        write32(totalFrames * 16); // 16 bytes per frame entry

        uint32_t offsetAccumulator = 4; // Start offset relative to 'movi' list internal data
        for (uint32_t i = 0; i < totalFrames; i++) {
            aviFile.write((const uint8_t*)"00dc", 4); // Chunk ID
            write32(0x10); // Flags: 0x10 means Keyframe (JPEGs are always keyframes)
            write32(offsetAccumulator);
            write32(frameSizes[i]); // Actual data length
            
            uint32_t paddedLen = (frameSizes[i] + 1) & ~1;
            offsetAccumulator += 8 + paddedLen;
        }
        
        uint32_t fileEndPos = aviFile.position();
        uint32_t totalFileSizeWithoutRiff = fileEndPos - 8;

        // 2. Seek back to rewrite the real headers
        aviFile.seek(0);

        // RIFF Header
        aviFile.write((const uint8_t*)"RIFF", 4);
        write32(totalFileSizeWithoutRiff);
        aviFile.write((const uint8_t*)"AVI ", 4);

        // LIST hdrl
        aviFile.write((const uint8_t*)"LIST", 4);
        write32(192); // Header list size
        aviFile.write((const uint8_t*)"hdrl", 4);

        // avih (Main AVI Header)
        aviFile.write((const uint8_t*)"avih", 4);
        write32(56); // Size of avih structure
        write32(1000000 / fps); // Microseconds per frame
        write32(0); // Max bytes per second (dummy)
        write32(0); // Padding granularity
        write32(0x10); // Flags (AVIF_HASINDEX)
        write32(totalFrames);
        write32(0); // Initial frames
        write32(1); // Streams
        write32(0); // Suggested buffer size
        write32(width);
        write32(height);
        uint32_t reserved[4] = {0};
        aviFile.write((uint8_t*)reserved, 16);

        // LIST strl (Stream List)
        aviFile.write((const uint8_t*)"LIST", 4);
        write32(116);
        aviFile.write((const uint8_t*)"strl", 4);

        // strh (Stream Header)
        aviFile.write((const uint8_t*)"strh", 4);
        write32(56);
        aviFile.write((const uint8_t*)"vids", 4); // Stream Type: Video
        aviFile.write((const uint8_t*)"MJPG", 4); // Handler: Motion JPEG
        write32(0); // Flags
        write16(0); // Priority
        write16(0); // Language
        write32(0); // Initial frames
        write32(1); // Scale
        write32(fps); // Rate
        write32(0); // Start
        write32(totalFrames); // Length
        write32(1024 * 64); // Suggested buffer size
        write32(-1); // Quality
        write32(0); // Sample size
        write16(0); // Frame left
        write16(0); // Frame top
        write16(width); // Frame right
        write16(height); // Frame bottom

        // strf (Stream Format)
        aviFile.write((const uint8_t*)"strf", 4);
        write32(40); // Size of BITMAPINFOHEADER
        write32(40); // Size
        write32(width);
        write32(height);
        write16(1); // Planes
        write16(24); // Bit count (24 bit RGB representation for JPEG container)
        aviFile.write((const uint8_t*)"MJPG", 4); // Compression
        write32(width * height * 3); // Image size
        write32(0); // XPelsPerMeter
        write32(0); // YPelsPerMeter
        write32(0); // Colors used
        write32(0); // Colors important

        // LIST movi
        aviFile.write((const uint8_t*)"LIST", 4);
        write32(moviSize + 4);
        aviFile.write((const uint8_t*)"movi", 4);

        // Close file handles
        aviFile.close();
        if (frameSizes) {
            free(frameSizes);
            frameSizes = nullptr;
        }
    }
};
#endif
