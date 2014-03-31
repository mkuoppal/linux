/*
 * Copyright © 2014 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Mika Kuoppala <mika.kuoppala@intel.com>
 *
 */

#include "i915_drv.h"

#define BATCH_SIZE 4096

struct i915_render_state_obj {
	struct drm_i915_gem_object *obj;
	u32 *base;
	u32 size;
	u32 len;
};

static struct i915_render_state_obj *
render_state_obj_alloc(struct drm_device *dev)
{
	struct i915_render_state_obj *so;
	int ret;

	so = kzalloc(sizeof(*so), GFP_KERNEL);
	if (!so)
		return ERR_PTR(-ENOMEM);

	so->size = BATCH_SIZE;

	so->obj = i915_gem_alloc_object(dev, so->size);
	if (so->obj == NULL) {
		ret = -ENOMEM;
		goto free;
	}

	ret = i915_gem_obj_ggtt_pin(so->obj, 4096, 0);
	if (ret)
		goto free_gem;

	so->base = i915_gem_vmap_obj(so->obj);
	if (!so->base) {
		ret = -ENOMEM;
		goto unpin;
	}

	return so;
unpin:
	i915_gem_object_ggtt_unpin(so->obj);
free_gem:
	drm_gem_object_unreference(&so->obj->base);
free:
	kfree(so);
	return ERR_PTR(ret);
}

static void render_state_obj_free(struct i915_render_state_obj *so)
{
	vunmap(so->base);
	i915_gem_object_ggtt_unpin(so->obj);
	drm_gem_object_unreference(&so->obj->base);
	kfree(so);
}

static int gen6_generate_batch(struct i915_render_state_obj *so)
{
	unsigned int i = 0;
	u32 *b = so->base;

	b[i++] = MI_BATCH_BUFFER_END;
	so->len = i * 4;

	return 0;
}

static int gen7_generate_batch(struct i915_render_state_obj *so)
{
	unsigned int i = 0;
	u32 *b = so->base;

	b[i++] = MI_BATCH_BUFFER_END;
	so->len = i * 4;

	return 0;
}

static int gen8_generate_batch(struct i915_render_state_obj *so)
{
	unsigned int i = 0;
	u32 *b = so->base;

	b[i++] = MI_BATCH_BUFFER_END;
	so->len = i * 4;

	return 0;
}

int i915_gem_init_render_state(struct intel_ring_buffer *ring)
{
	const int gen = INTEL_INFO(ring->dev)->gen;
	struct i915_render_state_obj *so;
	u32 seqno;
	int ret;

	if (gen < 6)
		return 0;

	so = render_state_obj_alloc(ring->dev);
	if (IS_ERR(so))
		return PTR_ERR(so);

	switch(gen) {
	case 6:
		ret = gen6_generate_batch(so);
		break;
	case 7:
		ret = gen7_generate_batch(so);
		break;
	case 8:
		ret = gen8_generate_batch(so);
		break;
	default:
		WARN(1, "gen %x render state not implemented\n", gen);
		ret = 0;
		goto out;
	}
	if (ret)
		goto out;

	ret = ring->dispatch_execbuffer(ring, i915_gem_obj_ggtt_offset(so->obj),
					so->len, I915_DISPATCH_SECURE);
	if (ret)
		goto out;

	ret = intel_ring_flush_all_caches(ring);
	if (ret)
		goto out;

	ret = i915_add_request(ring, &seqno);
	if (ret)
		goto out;

	ret = i915_wait_seqno(ring, seqno);
out:
	render_state_obj_free(so);
	return ret;
}
