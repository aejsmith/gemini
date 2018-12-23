/*
 * Copyright (C) 2018 Alex Smith
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

#include "Render/RenderLayer.h"

#include "Render/RenderOutput.h"

RenderLayer::RenderLayer(const uint8_t inOrder) :
    mOrder  (inOrder),
    mOutput (nullptr),
    mActive (false)
{
}

RenderLayer::~RenderLayer()
{
    if (mActive)
    {
        DeactivateLayer();
    }
}

void RenderLayer::SetLayerOutput(RenderOutput* const inOutput)
{
    const bool active = mActive;

    if (active)
    {
        DeactivateLayer();
    }

    mOutput = inOutput;

    if (active)
    {
        ActivateLayer();
    }
}

void RenderLayer::ActivateLayer()
{
    Assert(mOutput);
    Assert(!mActive);

    mOutput->RegisterLayer(this, {});

    mActive = true;
}

void RenderLayer::DeactivateLayer()
{
    Assert(mOutput);
    Assert(mActive);

    mOutput->UnregisterLayer(this, {});

    mActive = false;
}
