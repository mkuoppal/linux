/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Copyright 2014 Intel Corporation
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "intel_batchbuffer.h"

static uint8_t *intel_batch_state_start(struct intel_batchbuffer *batch)
{
	uint32_t off = batch->size >> 1;

	off = ALIGN(off, 4);

	return &batch->base[off];
}

void intel_batch_reset(struct drm_device *dev,
		       struct intel_batchbuffer *batch,
		       void *p,
		       uint32_t size,
		       unsigned long ggtt_offset)
{
	batch->dev = dev;
	batch->base = batch->base_ptr = p;
	batch->err = 0;
	batch->size = size;
	batch->state_base = batch->state_ptr = intel_batch_state_start(batch);
	batch->ggtt_offset = ggtt_offset;
}

uint32_t intel_batch_used(struct intel_batchbuffer *batch)
{
	return batch->state_ptr - batch->base;
}

void *intel_batch_state_alloc(struct intel_batchbuffer *batch,
			      uint32_t size,
			      uint32_t align)
{
	uint32_t cur;
	uint32_t offset;

	if (batch->err)
		return NULL;

	cur  = intel_batch_used(batch);
	offset = ALIGN(cur, align);

	if (offset + size >= batch->size) {
		batch->err = -ENOSPC;
		return NULL;
	}

	batch->state_ptr = batch->base + offset + size;

	return memset(batch->base + cur, 0, size);
}

int intel_batch_offset(struct intel_batchbuffer *batch, const void *ptr)
{
	return (uint8_t *)ptr - batch->base;
}

int intel_batch_state_copy(struct intel_batchbuffer *batch,
			   const void *ptr,
			   const uint32_t size,
			   const uint32_t align)
{
	void * const p = intel_batch_state_alloc(batch, size, align);

	if (p == NULL)
		return -1;

	return intel_batch_offset(batch, memcpy(p, ptr, size));
}

static uint32_t intel_batch_space(struct intel_batchbuffer *batch)
{
	return batch->state_base - batch->base_ptr;
}

void intel_batch_emit_dword(struct intel_batchbuffer *batch, uint32_t dword)
{
	if (batch->err)
		return;

	if (intel_batch_space(batch) < 4) {
		batch->err = -ENOSPC;
		return;
	}

	*(uint32_t *) (batch->base_ptr) = dword;
	batch->base_ptr += 4;
}

void intel_batch_emit_reloc(struct intel_batchbuffer *batch,
			    const uint32_t delta)
{
	if (batch->err)
		return;

	if (delta >= batch->size) {
		batch->err = -EINVAL;
		return;
	}

	intel_batch_emit_dword(batch, batch->ggtt_offset + delta);
}
