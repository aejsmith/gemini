/*
 * Copyright (C) 2018-2019 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "Loaders/TextureLoader.h"

class TGALoader final : public Texture2DLoader
{
    CLASS();

public:
    const char*                 GetExtension() const override
                                    { return "tga"; }

protected:
                                ~TGALoader() {}

    bool                        LoadData() override;

private:
    #pragma pack(push, 1)
    struct Header
    {
        uint8_t                 idLength;
        uint8_t                 colourMapType;
        uint8_t                 imageType;
        uint16_t                colourMapOrigin;
        uint16_t                colourMapLength;
        uint8_t                 colourMapDepth;
        uint16_t                xOrigin;
        uint16_t                yOrigin;
        uint16_t                width;
        uint16_t                height;
        uint8_t                 depth;
        uint8_t                 imageDescriptor;
    };
    #pragma pack(pop)
};
