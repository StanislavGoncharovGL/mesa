/*
 * © Copyright 2018 Alyssa Rosenzweig
 * Copyright (C) 2019 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <panfrost-misc.h>
#include <panfrost-job.h>
#include "pan_context.h"

/* TODO: What does this actually have to be? */
#define ALIGNMENT 128

/* Transient command stream pooling: command stream uploads try to simply copy
 * into whereever we left off. If there isn't space, we allocate a new entry
 * into the pool and copy there */

struct panfrost_transfer
panfrost_allocate_transient(struct panfrost_context *ctx, size_t sz)
{
        struct panfrost_screen *screen = pan_screen(ctx->base.screen);
        struct panfrost_batch *batch = panfrost_get_batch_for_fbo(ctx);

        /* Pad the size */
        sz = ALIGN_POT(sz, ALIGNMENT);

        /* Find or create a suitable BO */
        struct panfrost_bo *bo = NULL;

        unsigned offset = 0;

        bool fits_in_current = (batch->transient_offset + sz) < TRANSIENT_SLAB_SIZE;

        if (likely(batch->transient_bo && fits_in_current)) {
                /* We can reuse the current BO, so get it */
                bo = batch->transient_bo;

                /* Use the specified offset */
                offset = batch->transient_offset;
                batch->transient_offset = offset + sz;
        } else {
                size_t bo_sz = sz < TRANSIENT_SLAB_SIZE ?
                               TRANSIENT_SLAB_SIZE : ALIGN_POT(sz, 4096);

                /* We can't reuse the current BO, but we can create a new one. */
                bo = panfrost_drm_create_bo(screen, bo_sz, 0);
                panfrost_batch_add_bo(batch, bo);

                /* Creating a BO adds a reference, and then the job adds a
                 * second one. So we need to pop back one reference */
                panfrost_bo_unreference(&screen->base, bo);

                if (sz < TRANSIENT_SLAB_SIZE) {
                        batch->transient_bo = bo;
                        batch->transient_offset = offset + sz;
                }
        }

        struct panfrost_transfer ret = {
                .cpu = bo->cpu + offset,
                .gpu = bo->gpu + offset,
        };

        return ret;

}

mali_ptr
panfrost_upload_transient(struct panfrost_context *ctx, const void *data, size_t sz)
{
        struct panfrost_transfer transfer = panfrost_allocate_transient(ctx, sz);
        memcpy(transfer.cpu, data, sz);
        return transfer.gpu;
}
