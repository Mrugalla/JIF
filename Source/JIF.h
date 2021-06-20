#pragma once
#include <JuceHeader.h>

namespace jif {
    struct Image {
        Image() :
            image(),
            x(0),
            y(0),
            width(0),
            height(0)
        {}
        Image(const juce::Image&& img) :
            image(img),
            x(0),
            y(0),
            width(static_cast<float>(img.getWidth())),
            height(static_cast<float>(img.getHeight()))
        {}
        void paint(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
            const auto nX = x * bounds.getWidth();
            const auto nY = y * bounds.getHeight();
            const auto nWidth = width * bounds.getWidth();
            const auto nHeight = height * bounds.getHeight();
            g.drawImage(image, { nX, nY, nWidth, nHeight });
        }
        juce::Image image;
        float x, y, width, height;
    };

    class Format
    {
        struct Loader
        {
            Loader(juce::InputStream& in) :
                image(),
                bgColour(0xff000000),
                input(in),
                dataBlockIsZero(false), fresh(false), finished(false),
                imageWidth(0), imageHeight(0), transparent(-1), numColours(0),
                currentBit(0), lastBit(0), lastByteIndex(0),
                codeSize(0), setCodeSize(0), maxCode(0), maxCodeSize(0),
                firstcode(0), oldcode(0), clearCode(0), endCode(0),
                sp(nullptr)
            {}

            bool readHeader() {
                if (!getSizeFromHeader())
                    return false;

                if (input.read(buf, 3) != 3)
                    return false;

                numColours = 2 << (buf[0] & 7);

                if ((buf[0] & 0x80) == 0)
                    return false;

                readPalette();

                const auto bgColourIdx = buf[1];
                bgColour = juce::Colour(palette[bgColourIdx]);
                return true;
            }

            void loadAnotherImage() {
                for (;;) {
                    if (input.read(buf, 1) != 1 || buf[0] == ';') break;

                    if (buf[0] == '!') {
                        if (readExtension())
                            continue;
                        break;
                    }

                    if (buf[0] != ',') continue;

                    if (input.read(buf, 9) == 9) {
                        const auto imageX = (int)juce::ByteOrder::littleEndianShort(buf + 0);
                        const auto imageY = (int)juce::ByteOrder::littleEndianShort(buf + 2);
                        imageWidth = (int)juce::ByteOrder::littleEndianShort(buf + 4);
                        imageHeight = (int)juce::ByteOrder::littleEndianShort(buf + 6);

                        numColours = 2 << (buf[8] & 7);

                        if ((buf[8] & 0x80) != 0)
                            readPalette();

                        const auto pxlFormat = transparent >= 0 ? juce::Image::ARGB : juce::Image::RGB;
                        image = juce::Image(pxlFormat, imageWidth, imageHeight, transparent >= 0);
                        image.x = static_cast<float>(imageX);
                        image.y = static_cast<float>(imageY);

                        image.image.getProperties()->set("originalImageHadAlpha", transparent >= 0);

                        readImage((buf[8] & 0x40) != 0);
                    }

                    break;
                }
            }

            Image image;
            juce::Colour bgColour;
        private:
            juce::InputStream& input;
            
            bool dataBlockIsZero, fresh, finished;
            int imageWidth, imageHeight, transparent, numColours;
            int currentBit, lastBit, lastByteIndex;
            int codeSize, setCodeSize;
            int maxCode, maxCodeSize;
            int firstcode, oldcode;
            int clearCode, endCode;
            enum { maxGifCode = 1 << 12 };
            int* sp;
            juce::uint8 buffer[260];
            juce::uint8 buf[16];
            juce::PixelARGB palette[256];
            int table[2][maxGifCode];
            int stack[2 * maxGifCode];

            bool getSizeFromHeader() {
                char b[6];

                if (input.read(b, 6) == 6
                    && (strncmp("GIF87a", b, 6) == 0
                        || strncmp("GIF89a", b, 6) == 0)) {
                    if (input.read(b, 4) == 4) {
                        imageWidth = (int)juce::ByteOrder::littleEndianShort(b);
                        imageHeight = (int)juce::ByteOrder::littleEndianShort(b + 2);
                        return imageWidth > 0 && imageHeight > 0;
                    }
                }

                return false;
            }

            void readPalette() {
                for (int i = 0; i < numColours; ++i) {
                    juce::uint8 rgb[4];
                    input.read(rgb, 3);
                    palette[i].setARGB(0xff, rgb[0], rgb[1], rgb[2]);
                    palette[i].premultiply();
                }
            }

            int readDataBlock(juce::uint8* const dest) {
                juce::uint8 n;
                if (input.read(&n, 1) == 1) {
                    dataBlockIsZero = (n == 0);

                    if (dataBlockIsZero || (input.read(dest, n) == n))
                        return n;
                }

                return -1;
            }

            int readExtension() {
                juce::uint8 type;
                if (input.read(&type, 1) != 1)
                    return false;

                juce::uint8 b[260];
                int n = 0;

                if (type == 0xf9) {
                    n = readDataBlock(b);
                    if (n < 0)
                        return 1;

                    if ((b[0] & 1) != 0)
                        transparent = b[3];
                }

                do {
                    n = readDataBlock(b);
                } while (n > 0);

                return n >= 0;
            }

            void clearTable()
            {
                int i;
                for (i = 0; i < clearCode; ++i) {
                    table[0][i] = 0;
                    table[1][i] = i;
                }

                for (; i < maxGifCode; ++i) {
                    table[0][i] = 0;
                    table[1][i] = 0;
                }
            }

            void initialise(const int inputCodeSize)
            {
                setCodeSize = inputCodeSize;
                codeSize = setCodeSize + 1;
                clearCode = 1 << setCodeSize;
                endCode = clearCode + 1;
                maxCodeSize = 2 * clearCode;
                maxCode = clearCode + 2;

                getCode(0, true);

                fresh = true;
                clearTable();
                sp = stack;
            }

            int readLZWByte()
            {
                if (fresh)
                {
                    fresh = false;

                    for (;;)
                    {
                        firstcode = oldcode = getCode(codeSize, false);

                        if (firstcode != clearCode)
                            return firstcode;
                    }
                }

                if (sp > stack)
                    return *--sp;

                int code;

                while ((code = getCode(codeSize, false)) >= 0)
                {
                    if (code == clearCode)
                    {
                        clearTable();
                        codeSize = setCodeSize + 1;
                        maxCodeSize = 2 * clearCode;
                        maxCode = clearCode + 2;
                        sp = stack;
                        firstcode = oldcode = getCode(codeSize, false);
                        return firstcode;
                    }
                    else if (code == endCode)
                    {
                        if (dataBlockIsZero)
                            return -2;

                        juce::uint8 buff[260];
                        int n;

                        while ((n = readDataBlock(buff)) > 0)
                        {
                        }

                        if (n != 0)
                            return -2;
                    }

                    const int incode = code;

                    if (code >= maxCode)
                    {
                        *sp++ = firstcode;
                        code = oldcode;
                    }

                    while (code >= clearCode)
                    {
                        *sp++ = table[1][code];
                        if (code == table[0][code])
                            return -2;

                        code = table[0][code];
                    }

                    *sp++ = firstcode = table[1][code];

                    if ((code = maxCode) < maxGifCode)
                    {
                        table[0][code] = oldcode;
                        table[1][code] = firstcode;
                        ++maxCode;

                        if (maxCode >= maxCodeSize && maxCodeSize < maxGifCode)
                        {
                            maxCodeSize <<= 1;
                            ++codeSize;
                        }
                    }

                    oldcode = incode;

                    if (sp > stack)
                        return *--sp;
                }

                return code;
            }

            int getCode(const int codeSize_, const bool shouldInitialise)
            {
                if (shouldInitialise)
                {
                    currentBit = 0;
                    lastBit = 0;
                    finished = false;
                    return 0;
                }

                if ((currentBit + codeSize_) >= lastBit)
                {
                    if (finished)
                        return -1;

                    buffer[0] = buffer[lastByteIndex - 2];
                    buffer[1] = buffer[lastByteIndex - 1];

                    const int n = readDataBlock(buffer + 2);

                    if (n == 0)
                        finished = true;

                    lastByteIndex = 2 + n;
                    currentBit = (currentBit - lastBit) + 16;
                    lastBit = (2 + n) * 8;
                }

                int result = 0;
                int i = currentBit;

                for (int j = 0; j < codeSize_; ++j)
                {
                    result |= ((buffer[i >> 3] & (1 << (i & 7))) != 0) << j;
                    ++i;
                }

                currentBit += codeSize_;
                return result;
            }

            bool readImage(const int interlace) {
                juce::uint8 c;
                if (input.read(&c, 1) != 1)
                    return false;

                initialise(c);

                if (transparent >= 0)
                    palette[transparent].setARGB(0, 0, 0, 0);

                int xpos = 0, ypos = 0, yStep = 8, pass = 0;

                const juce::Image::BitmapData destData(image.image, juce::Image::BitmapData::writeOnly);
                juce::uint8* p = destData.getPixelPointer(0, 0);
                const bool hasAlpha = image.image.hasAlphaChannel();

                for (;;) {
                    const int index = readLZWByte();
                    if (index < 0)
                        break;

                    if (hasAlpha)
                        ((juce::PixelARGB*)p)->set(palette[index]);
                    else
                        ((juce::PixelRGB*)p)->set(palette[index]);

                    p += destData.pixelStride;

                    if (++xpos == destData.width) {
                        xpos = 0;

                        if (interlace) {
                            ypos += yStep;

                            while (ypos >= destData.height) {
                                switch (++pass)
                                {
                                case 1:     ypos = 4; yStep = 8; break;
                                case 2:     ypos = 2; yStep = 4; break;
                                case 3:     ypos = 1; yStep = 2; break;
                                default:    return true;
                                }
                            }
                        }
                        else {
                            if (++ypos >= destData.height)
                                break;
                        }

                        p = destData.getPixelPointer(xpos, ypos);
                    }
                }

                return true;
            }

            JUCE_DECLARE_NON_COPYABLE(Loader)
        };
    public:
        bool valid(juce::InputStream& in) {
#if (JUCE_MAC || JUCE_IOS) && USE_COREGRAPHICS_RENDERING && JUCE_USE_COREIMAGE_LOADER
            return false;
#else
            loader = std::make_unique<Loader>(in);
            return loader->readHeader();
#endif
        }

        Image decodeImage() {
            loader->loadAnotherImage();
            return loader->image;
        };

        juce::Colour getBackgroundColour() { return loader->bgColour; }

        std::unique_ptr<Loader> loader;
    };

    struct JIF {
        JIF() :
            images(),
            bgColour(0x00000000),
            readIdx(0),
            lastReadIdx(-1),
            loopStart(0),
            loopEnd(0),
            startIdx(0)
        {
        }
        JIF(const void* jifData, const size_t jifSize) :
            images(),
            bgColour(0x00000000),
            readIdx(0),
            lastReadIdx(0),
            loopStart(0),
            loopEnd(0),
            startIdx(0)
        { reload(jifData, jifSize); }
        void reload(const void* jifData, const size_t jifSize) {
            images.clear();
            juce::MemoryInputStream memoryInputStream(jifData, jifSize, true);
            Format format;
            if (format.valid(memoryInputStream)) {
                bgColour = format.getBackgroundColour();
                while (!memoryInputStream.isExhausted()) {
                    const auto img = format.decodeImage();
                    if (img.image.isValid())
                        images.push_back(img);
                }
            }
            else return;
            auto maxWidth = 0.f;
            auto maxHeight = 0.f;
            for (const auto& img : images) {
                const auto w = img.image.getWidth();
                const auto h = img.image.getHeight();
                maxWidth = maxWidth < w ? w : maxWidth;
                maxHeight = maxHeight < h ? h : maxHeight;
            }

            for (auto& img : images) {
                img.x /= maxWidth;
                img.y /= maxHeight;
                img.width /= maxWidth;
                img.height /= maxHeight;
            }
            
            loopStart = startIdx = 0;
            loopEnd = numImages();
            lastReadIdx = -1;
        }
        const size_t numImages() const noexcept { return images.size(); }
        void paint(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
            if (!images.empty()) {
                if (lastReadIdx < readIdx)
                    for (auto i = lastReadIdx + 1; i <= readIdx; ++i)
                        images[i].paint(g, bounds);
                else if (lastReadIdx > readIdx) {
                    g.fillAll(bgColour);
                    for (auto i = 0; i <= readIdx; ++i)
                        images[i].paint(g, bounds);
                }
                lastReadIdx = readIdx;
                return;
            }
            g.fillAll(juce::Colours::black);
            g.setColour(juce::Colours::white);
            g.drawFittedText("Click here\nto import a\nJIF!", bounds.toNearestInt(), juce::Justification::centred, 1);
        }
        void operator++() { ++readIdx; if (readIdx >= loopEnd) readIdx = loopStart; }
        void resetAnimation() { readIdx = 0; }
        /* returns true if should repaint (readIdx != newReadIdx && numImages() != 0) */
        bool setFrameTo(float phase, const float offset) noexcept {
            if (numImages() == 0) return false;
            const auto range = static_cast<float>(loopEnd - loopStart);
            phase = loopStart + (phase + offset) * range;
            while (phase >= loopEnd) phase -= range;
            const auto newReadIdx = static_cast<int>(phase);
            if (readIdx == newReadIdx) return false;
            readIdx = newReadIdx;
            return true;
        }
        bool setFrameTo(const int imgIdx) noexcept {
            const bool changed = readIdx != imgIdx;
            if(changed)
                readIdx = imgIdx;
            return changed;
        }
        const bool empty() const noexcept { return images.empty(); }

        std::vector<Image> images;
        juce::Colour bgColour;
        int readIdx, lastReadIdx, loopStart, loopEnd, startIdx;
    };
}