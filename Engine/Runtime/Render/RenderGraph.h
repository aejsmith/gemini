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

#include "Engine/FrameAllocator.h"

#include "Render/RenderDefs.h"

#include <functional>

class GPUBuffer;
class GPUComputeCommandList;
class GPUGraphicsCommandList;
class GPUResourceView;
class GPUStagingBuffer;
class GPUTexture;
class GPUTransferContext;
class RenderGraph;
class RenderGraphPass;
class RenderGraphWindow;
class RenderLayer;
class RenderOutput;

struct GPUBufferDesc;
struct GPUTextureDesc;

enum RenderGraphPassType
{
    kRenderGraphPassType_Render,
    kRenderGraphPassType_Compute,
    kRenderGraphPassType_Transfer,
};

enum RenderResourceType
{
    kRenderResourceType_Texture,
    kRenderResourceType_Buffer,
};

/**
 * Handle to a resource. This is a small opaque type intended to be passed by
 * value.
 */
struct RenderResourceHandle
{
public:
                                    RenderResourceHandle();

                                    operator bool() const;

private:
    uint16_t                        index;
    uint16_t                        version;

    friend class RenderGraph;
    friend class RenderGraphPass;
    friend class RenderGraphWindow;
};

inline RenderResourceHandle::RenderResourceHandle() :
    index   (std::numeric_limits<uint16_t>::max()),
    version (0)
{
}

inline RenderResourceHandle::operator bool() const
{
    return index != std::numeric_limits<uint16_t>::max();
}

/**
 * Descriptor for a texture render graph resource. Similar to GPUTextureDesc,
 * except omits usage. Required usage flags are automatically derived by the
 * graph from the requirements of all passes that use the resource.
 */
struct RenderTextureDesc
{
    const char*                     name            = nullptr;

    GPUResourceType                 type            = kGPUResourceType_Texture2D;
    GPUTextureFlags                 flags           = kGPUTexture_None;
    PixelFormat                     format          = kPixelFormat_Unknown;

    uint32_t                        width           = 1;
    uint32_t                        height          = 1;
    uint32_t                        depth           = 1;

    uint16_t                        arraySize       = 1;
    uint8_t                         numMipLevels    = 1;
};

/**
 * Descriptor for a buffer render graph resource. As with RenderTextureDesc,
 * similar to GPUBufferDesc but omits usage.
 */
struct RenderBufferDesc
{
    const char*                     name            = nullptr;

    size_t                          size            = 1;
};

/**
 * Descriptor for a view of a render graph resource. Similar to
 * GPUResourceViewDesc, but instead of a usage flag, specifies a resource
 * state (see below).
 */
struct RenderViewDesc
{
    GPUResourceViewType             type;

    /**
     * Resource state that the subresource range will need to be in when this
     * view is used. Usage flag for the GPUResourceView is derived from this.
     * Must only include states for compatible usages (e.g. same type of read
     * usage in multiple shader stages is allowed).
     */
    GPUResourceState                state;

    /**
     * Format for the view. For texture views, if this is left as unknown,
     * will be automatically set to match the underlying texture.
     */
    PixelFormat                     format          = kPixelFormat_Unknown;

    uint32_t                        mipOffset       = 0;
    uint32_t                        mipCount        = 1;
    uint32_t                        elementOffset   = 0;
    uint32_t                        elementCount    = 1;
};

/**
 * Handle to a view within a pass, specific to that pass. This is a small
 * opaque type intended to be passed by value.
 */
struct RenderViewHandle
{
public:
                                    RenderViewHandle();

                                    operator bool() const;

private:
    RenderGraphPass*                pass;
    uint16_t                        index;

    friend class RenderGraph;
    friend class RenderGraphPass;
    friend class RenderGraphWindow;
};

inline RenderViewHandle::RenderViewHandle() :
    index (std::numeric_limits<uint16_t>::max())
{
}

inline RenderViewHandle::operator bool() const
{
    return index != std::numeric_limits<uint16_t>::max();
}

class RenderGraphPass : Uncopyable
{
public:
    using RenderFunction          = std::function<void (const RenderGraph&,
                                                        const RenderGraphPass&,
                                                        GPUGraphicsCommandList&)>;

    using ComputeFunction         = std::function<void (const RenderGraph&,
                                                        const RenderGraphPass&,
                                                        GPUComputeCommandList&)>;

    using TransferFunction        = std::function<void (const RenderGraph&,
                                                        const RenderGraphPass&,
                                                        GPUTransferContext&)>;

public:
    /**
     * Set the function for executing the pass. Must use the type appropriate
     * to the type of the pass. The function will be executed on the main
     * thread.
     */
    void                            SetFunction(RenderFunction inFunction);
    void                            SetFunction(ComputeFunction inFunction);
    void                            SetFunction(TransferFunction inFunction);

    /**
     * Declare usage of a resource in the pass. This is to be used when the
     * usage does not require a view to be created. When a view is needed, use
     * one of the view creation methods instead.
     *
     * If the specified resource state is writeable, and outNewHandle is non-
     * null, a new handle referring to the resource after the pass will be
     * returned there. It is valid to pass null if the resource is writeable
     * but it is not needed after the pass, e.g. it is just transient storage.
     */
    void                            UseResource(const RenderResourceHandle inHandle,
                                                const GPUSubresourceRange& inRange,
                                                const GPUResourceState     inState,
                                                RenderResourceHandle*      outNewHandle = nullptr);

    /**
     * Create a view to use a resource within the pass. See UseResource()
     * regarding outNewHandle.
     */
    RenderViewHandle                CreateView(const RenderResourceHandle inHandle,
                                               const RenderViewDesc&      inDesc,
                                               RenderResourceHandle*      outNewHandle = nullptr);

    /**
     * For a render pass, creates a view of a resource and sets this as a
     * colour attachment for the pass. The first version is a shortcut which
     * will just target level/layer 0 of the resource. The second allows more
     * control by providing view properties. The state must be set to
     * kGPUResourceState_RenderTarget. outNewHandle must be non-null for both
     * uses since a colour target is a write access, and there is no use in
     * writing to a colour target and not consuming the output.
     *
     * The render pass load/store ops will be configured automatically by
     * default. If the subresource has no previous writes, the load op will be
     * set to discard, otherwise it will be set to load. Use the Clear*()
     * methods to clear to an explicit value rather than discarding. The store
     * op will be set to discard if no subsequent passes use the resource after
     * the pass, otherwise it will be set to store.
     */
    void                            SetColour(const uint8_t              inIndex,
                                              const RenderResourceHandle inHandle,
                                              RenderResourceHandle*      outNewHandle);
    void                            SetColour(const uint8_t              inIndex,
                                              const RenderResourceHandle inHandle,
                                              const RenderViewDesc&      inDesc,
                                              RenderResourceHandle*      outNewHandle);

    /**
     * For a render pass, creates a view of a resource and sets this as the
     * depth/stencil attachment for the pass. See CreateColourView() regarding
     * load/store ops.  The first version is a shortcut which will just target
     * level/layer 0 of the resource. A depth/stencil resource state must be
     * specified which determines the writeability of the resource. The second
     * version allows more control by providing full view properties. If state
     * is kGPUResourceState_DepthStencilRead, then outNewHandle must be null.
     * Otherwise, it can be non-null if the contents will be needed after the
     * pass, but can still be null if the depth buffer is just transient.
     */
    void                            SetDepthStencil(const RenderResourceHandle inHandle,
                                                    const GPUResourceState     inState,
                                                    RenderResourceHandle*      outNewHandle = nullptr);
    void                            SetDepthStencil(const RenderResourceHandle inHandle,
                                                    const RenderViewDesc&      inDesc,
                                                    RenderResourceHandle*      outNewHandle = nullptr);

    /**
     * Clear an attachment to a specific value. If the attachment will always
     * be fully overwritten by the pass, do not use this: it will automatically
     * be discarded.
     *
     * It is an error to clear an attachment if the subresource has previous
     * writes, as this means the previous writes are useless. Where a cleared
     * resource is needed, declare a new resource rather than reusing an
     * existing one.
     */
    void                            ClearColour(const uint8_t    inIndex,
                                                const glm::vec4& inValue);
    void                            ClearDepth(const float inValue);
    void                            ClearStencil(const uint32_t inValue);

    /**
     * Force this pass to execute even if none of its outputs are consumed.
     * Useful during development (e.g. when writing a new pass where the
     * consumers aren't implemented yet), shouldn't be used otherwise.
     */
    void                            ForceRequired()
                                        { mRequired = true; }

    /**
     * Graph execution methods.
     */

    /** Retrieve a view from the pass. Only valid inside the pass function. */
    GPUResourceView*                GetView(const RenderViewHandle inHandle) const;

private:
    struct UsedResource
    {
        RenderResourceHandle        handle;
        GPUSubresourceRange         range;
        GPUResourceState            state;
    };

    struct View
    {
        RenderResourceHandle        resource;
        RenderViewDesc              desc;
        GPUResourceView*            view;
    };

    struct Attachment
    {
        RenderViewHandle            view;
        GPUTextureClearData         clearData;
    };

private:
                                    RenderGraphPass(RenderGraph&              inGraph,
                                                    std::string               inName,
                                                    const RenderGraphPassType inType,
                                                    const RenderLayer*        inLayer);
                                    ~RenderGraphPass() {}

private:
    RenderGraph&                    mGraph;
    const std::string               mName;
    const RenderGraphPassType       mType;
    const RenderLayer* const        mLayer;

    bool                            mRequired;

    std::vector<UsedResource>       mUsedResources;
    std::vector<View>               mViews;

    RenderFunction                  mRenderFunction;
    ComputeFunction                 mComputeFunction;
    TransferFunction                mTransferFunction;

    Attachment                      mColour[kMaxRenderPassColourAttachments];
    Attachment                      mDepthStencil;

    friend class RenderGraph;
    friend class RenderGraphWindow;
};

using RenderGraphPassArray = std::vector<RenderGraphPass*>;

inline void RenderGraphPass::SetFunction(RenderFunction inFunction)
{
    Assert(mType == kRenderGraphPassType_Render);
    mRenderFunction = std::move(inFunction);
}

inline void RenderGraphPass::SetFunction(ComputeFunction inFunction)
{
    Assert(mType == kRenderGraphPassType_Compute);
    mComputeFunction = std::move(inFunction);
}

inline void RenderGraphPass::SetFunction(TransferFunction inFunction)
{
    Assert(mType == kRenderGraphPassType_Transfer);
    mTransferFunction = std::move(inFunction);
}

/**
 * Rendering is driven by the render graph. To render the content of a
 * RenderOutput, each RenderLayer registered on it is visited in the defined
 * order to add the render passes (and declare the resources needed by those
 * passes) that they need to produce their output to the graph.
 *
 * Once all passes and resources have been declared (the build phase), we
 * determine the passes that are actually going to contribute to the final
 * output, and then execute those passes via the function that was given for
 * them (the execute phase).
 *
 * During the build phase, we only declare resources and how each pass will
 * use them; no memory is allocated. The real resources are allocated during
 * the execute phase automatically, and can be obtained in the pass function
 * through the graph from the handle that was given out in the build phase.
 *
 * Passes declare their usage of resources, including the resource state that
 * the resource must be in during that pass. Resource state transitions are
 * automatically performed when executing the graph. When a pass writes a
 * resource (it specifies a writeable state for its usage of the resource), a
 * new handle is produced. This handle refers to the content of the resource
 * after the pass has finished executing. Any subsequently added passes which
 * use the resource must use the new handle.
 *
 * There can only be one write access to a given resource handle. Read and
 * write to the same resource within a pass is only allowed on non-overlapping
 * subresource ranges (this would allow, for example, a mip generation pass
 * which reads one mip level and writes the one below it).
 */
class RenderGraph : Uncopyable
{
public:
                                    RenderGraph();
                                    ~RenderGraph();

    RenderResourceType              GetResourceType(const RenderResourceHandle inHandle) const;
    const RenderBufferDesc&         GetBufferDesc(const RenderResourceHandle inHandle) const;
    const RenderTextureDesc&        GetTextureDesc(const RenderResourceHandle inHandle) const;

    /**
     * Graph build methods.
     */

    /**
     * Add a new pass to the graph. The name is for informational purposes,
     * it does not have to be unique but is recommended to be to make it easier
     * to identify passes.
     */
    RenderGraphPass&                AddPass(std::string               inName,
                                            const RenderGraphPassType inType);

    /** Shortcut to add a pass to just blit one texture subresource to another. */
    RenderGraphPass&                AddBlitPass(std::string                inName,
                                                const RenderResourceHandle inDestHandle,
                                                const GPUSubresource       inDestSubresource,
                                                const RenderResourceHandle inSourceHandle,
                                                const GPUSubresource       inSourceSubresource,
                                                RenderResourceHandle*      outNewHandle);

    /** Shortcut to add a pass to upload a buffer. */
    RenderGraphPass&                AddUploadPass(std::string                inName,
                                                  const RenderResourceHandle inDestHandle,
                                                  const uint32_t             inDestOffset,
                                                  GPUStagingBuffer           inSourceBuffer,
                                                  RenderResourceHandle*      outNewHandle);

    /**
     * Create a new buffer resource. The initial content will be undefined so
     * the first pass that uses it must write to it.
     */
    RenderResourceHandle            CreateBuffer(const RenderBufferDesc& inDesc);

    /**
     * Create a new texture resource. The initial content will be undefined so
     * the first pass that uses it must write to it.
     */
    RenderResourceHandle            CreateTexture(const RenderTextureDesc& inDesc);

    /**
     * Import an external resource into the graph. This is to be used for
     * resources which need to persist outside of the graph, since all
     * resources created within a graph are transient and will be lost after
     * the graph execution completes.
     *
     * A resource state must be specified: it is assumed that the resource is
     * in this state when the graph execution begins, and it will be returned
     * to that state when execution completes.
     *
     * Optional callback functions can be supplied which will be called (from
     * the main thread) before any passes which use the resource are executed,
     * and after all passes have executed. The begin callback will be called
     * before any views to the resource are created.
     */
    RenderResourceHandle            ImportResource(GPUResource* const        inResource,
                                                   const GPUResourceState    inState,
                                                   const char* const         inName,
                                                   std::function<void ()>    inBeginCallback = {},
                                                   std::function<void ()>    inEndCallback = {},
                                                   const RenderOutput* const inOutput = nullptr);

    /**
     * Allocate a transient object that needs to remain alive until graph
     * execution is completed and be properly destroyed via its destructor. It
     * will be allocated via the frame allocator, the destructor will be called
     * at the end of graph execution. Trivially destructible types can just
     * use the frame allocator directly
     */
    template <typename T, typename... Args>
    T*                              NewTransient(Args&&... inArgs);

    void                            SetCurrentLayer(const RenderLayer* const inLayer)
                                        { mCurrentLayer = inLayer; }

    /**
     * Graph execution methods.
     */

    void                            Execute();

    /**
     * Methods to retrieve the real resource from a handle inside a pass
     * function.
     */
    GPUBuffer*                      GetBuffer(const RenderResourceHandle inHandle) const;
    GPUTexture*                     GetTexture(const RenderResourceHandle inHandle) const;

private:
    struct Resource
    {
        RenderResourceType          type;

        /**
         * Layer that the resource was created inside. Resources are scoped to
         * layers, resource names within a layer must be unique.
         */
        const RenderLayer*          layer;

        union
        {
            RenderTextureDesc       texture;
            RenderBufferDesc        buffer;
        };

        GPUResourceUsage            usage;

        /** Current version, incremented on each write. */
        uint16_t                    currentVersion;

        /** Array of passes which produced each version. */
        RenderGraphPassArray        producers;

        /** Imported resource details. */
        GPUResourceState            originalState;
        const RenderOutput*         output;
        std::function<void ()>      beginCallback;
        std::function<void ()>      endCallback;

        /** Flags. */
        bool                        imported : 1;
        bool                        required : 1;
        bool                        begun : 1;

        /** First and last users, set by DetermineRequiredPasses(). */
        RenderGraphPass*            firstPass;
        RenderGraphPass*            lastPass;

        /** Execution phase state. */
        GPUResource*                resource;
        // TODO: Per-subresource state tracking.
        GPUResourceState            currentState;

        /** If this resource is the debug output, this contains a copy of it. */
        GPUResource*                debugResource;

    public:
                                    Resource();

        const char*                 GetName() const;
    };

    using Destructor              = std::function<void()>;

    /**
     * Key to identify a pass. The use of this is for the debug window to have
     * some persistent way to identify a pass - since all the graph structures
     * are transient we cannot just use a pointer to refer to a pass across
     * frames. Pass names are expected to be unique within a layer.
     */
    struct PassKey
    {
        const RenderLayer*          layer = nullptr;
        std::string                 name;
    };

    /**
     * Key to identify a resource, same as for PassKey. This can optionally
     * refer to a specific version of a resource by including a producer pass
     * name (also within the same layer).
     */
    struct ResourceKey
    {
        const RenderLayer*          layer = nullptr;
        const char*                 name  = nullptr;
        std::string                 versionProducer;

    public:
        bool                        IsValid() const { return name != nullptr; }
    };

private:
    void                            AddDestructor(Destructor inDestructor);

    void                            TransitionResource(Resource&                  inResource,
                                                       const GPUSubresourceRange& inRange,
                                                       const GPUResourceState     inState);

    void                            FlushBarriers();

    void                            MakeBufferDesc(const Resource* const inResource,
                                                   GPUBufferDesc&        outDesc);
    void                            MakeTextureDesc(const Resource* const inResource,
                                                    GPUTextureDesc&       outDesc);

    void                            DetermineRequiredPasses();
    void                            AllocateResources();
    void                            EndResources();
    void                            PrepareResources(RenderGraphPass& inPass);
    void                            CreateViews(RenderGraphPass& inPass);
    void                            DestroyViews(RenderGraphPass& inPass);
    void                            ExecutePass(RenderGraphPass& inPass);

    const RenderGraphPass*          FindPass(const PassKey& inKey) const;
    const Resource*                 FindResource(const ResourceKey& inKey) const;

private:
    RenderGraphPassArray            mPasses;
    std::vector<Resource*>          mResources;

    const RenderLayer*              mCurrentLayer;
    bool                            mIsExecuting;

    std::vector<GPUResourceBarrier> mBarriers;

    std::vector<Destructor>         mDestructors;

    /** Resource to display as debug output, controlled by GUI. */
    static ResourceKey              mDebugOutput;

    friend class RenderGraphPass;
    friend class RenderGraphWindow;
};

inline RenderResourceType RenderGraph::GetResourceType(const RenderResourceHandle inHandle) const
{
    Assert(inHandle.index < mResources.size());
    return mResources[inHandle.index]->type;
}

inline const RenderBufferDesc& RenderGraph::GetBufferDesc(const RenderResourceHandle inHandle) const
{
    Assert(GetResourceType(inHandle) == kRenderResourceType_Buffer);
    return mResources[inHandle.index]->buffer;
}

inline const RenderTextureDesc& RenderGraph::GetTextureDesc(const RenderResourceHandle inHandle) const
{
    Assert(GetResourceType(inHandle) == kRenderResourceType_Texture);
    return mResources[inHandle.index]->texture;
}

template <typename T, typename... Args>
inline T* RenderGraph::NewTransient(Args&&... inArgs)
{
    T* const result = FrameAllocator::New<T>(std::forward<Args>(inArgs)...);
    AddDestructor([result] () { FrameAllocator::Delete(result); });
    return result;
}
