/*
 * Copyright 2012 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#include "nvc0.h"
#include "ctxnvc0.h"

/*******************************************************************************
 * Zero Bandwidth Clear
 ******************************************************************************/

static void
nvc0_graph_zbc_clear_color(struct nvc0_graph_priv *priv, int zbc)
{
	if (priv->zbc_color[zbc].format) {
		nv_wr32(priv, 0x405804, priv->zbc_color[zbc].ds[0]);
		nv_wr32(priv, 0x405808, priv->zbc_color[zbc].ds[1]);
		nv_wr32(priv, 0x40580c, priv->zbc_color[zbc].ds[2]);
		nv_wr32(priv, 0x405810, priv->zbc_color[zbc].ds[3]);
	}
	nv_wr32(priv, 0x405814, priv->zbc_color[zbc].format);
	nv_wr32(priv, 0x405820, zbc);
	nv_wr32(priv, 0x405824, 0x00000004); /* TRIGGER | WRITE | COLOR */
}

static int
nvc0_graph_zbc_color_get(struct nvc0_graph_priv *priv, int format,
			 const u32 ds[4], const u32 l2[4])
{
	struct nouveau_ltc *ltc = nouveau_ltc(priv);
	int zbc = -ENOSPC, i;

	for (i = ltc->zbc_min; i <= ltc->zbc_max; i++) {
		if (priv->zbc_color[i].format) {
			if (priv->zbc_color[i].format != format)
				continue;
			if (memcmp(priv->zbc_color[i].ds, ds, sizeof(
				   priv->zbc_color[i].ds)))
				continue;
			if (memcmp(priv->zbc_color[i].l2, l2, sizeof(
				   priv->zbc_color[i].l2))) {
				WARN_ON(1);
				return -EINVAL;
			}
			return i;
		} else {
			zbc = (zbc < 0) ? i : zbc;
		}
	}

	if (zbc < 0)
		return zbc;

	memcpy(priv->zbc_color[zbc].ds, ds, sizeof(priv->zbc_color[zbc].ds));
	memcpy(priv->zbc_color[zbc].l2, l2, sizeof(priv->zbc_color[zbc].l2));
	priv->zbc_color[zbc].format = format;
	ltc->zbc_color_get(ltc, zbc, l2);
	nvc0_graph_zbc_clear_color(priv, zbc);
	return zbc;
}

static void
nvc0_graph_zbc_clear_depth(struct nvc0_graph_priv *priv, int zbc)
{
	if (priv->zbc_depth[zbc].format)
		nv_wr32(priv, 0x405818, priv->zbc_depth[zbc].ds);
	nv_wr32(priv, 0x40581c, priv->zbc_depth[zbc].format);
	nv_wr32(priv, 0x405820, zbc);
	nv_wr32(priv, 0x405824, 0x00000005); /* TRIGGER | WRITE | DEPTH */
}

static int
nvc0_graph_zbc_depth_get(struct nvc0_graph_priv *priv, int format,
			 const u32 ds, const u32 l2)
{
	struct nouveau_ltc *ltc = nouveau_ltc(priv);
	int zbc = -ENOSPC, i;

	for (i = ltc->zbc_min; i <= ltc->zbc_max; i++) {
		if (priv->zbc_depth[i].format) {
			if (priv->zbc_depth[i].format != format)
				continue;
			if (priv->zbc_depth[i].ds != ds)
				continue;
			if (priv->zbc_depth[i].l2 != l2) {
				WARN_ON(1);
				return -EINVAL;
			}
			return i;
		} else {
			zbc = (zbc < 0) ? i : zbc;
		}
	}

	if (zbc < 0)
		return zbc;

	priv->zbc_depth[zbc].format = format;
	priv->zbc_depth[zbc].ds = ds;
	priv->zbc_depth[zbc].l2 = l2;
	ltc->zbc_depth_get(ltc, zbc, l2);
	nvc0_graph_zbc_clear_depth(priv, zbc);
	return zbc;
}

/*******************************************************************************
 * Graphics object classes
 ******************************************************************************/

static int
nvc0_fermi_mthd_zbc_color(struct nouveau_object *object, void *data, u32 size)
{
	struct nvc0_graph_priv *priv = (void *)object->engine;
	union {
		struct fermi_a_zbc_color_v0 v0;
	} *args = data;
	int ret;

	if (nvif_unpack(args->v0, 0, 0, false)) {
		switch (args->v0.format) {
		case FERMI_A_ZBC_COLOR_V0_FMT_ZERO:
		case FERMI_A_ZBC_COLOR_V0_FMT_UNORM_ONE:
		case FERMI_A_ZBC_COLOR_V0_FMT_RF32_GF32_BF32_AF32:
		case FERMI_A_ZBC_COLOR_V0_FMT_R16_G16_B16_A16:
		case FERMI_A_ZBC_COLOR_V0_FMT_RN16_GN16_BN16_AN16:
		case FERMI_A_ZBC_COLOR_V0_FMT_RS16_GS16_BS16_AS16:
		case FERMI_A_ZBC_COLOR_V0_FMT_RU16_GU16_BU16_AU16:
		case FERMI_A_ZBC_COLOR_V0_FMT_RF16_GF16_BF16_AF16:
		case FERMI_A_ZBC_COLOR_V0_FMT_A8R8G8B8:
		case FERMI_A_ZBC_COLOR_V0_FMT_A8RL8GL8BL8:
		case FERMI_A_ZBC_COLOR_V0_FMT_A2B10G10R10:
		case FERMI_A_ZBC_COLOR_V0_FMT_AU2BU10GU10RU10:
		case FERMI_A_ZBC_COLOR_V0_FMT_A8B8G8R8:
		case FERMI_A_ZBC_COLOR_V0_FMT_A8BL8GL8RL8:
		case FERMI_A_ZBC_COLOR_V0_FMT_AN8BN8GN8RN8:
		case FERMI_A_ZBC_COLOR_V0_FMT_AS8BS8GS8RS8:
		case FERMI_A_ZBC_COLOR_V0_FMT_AU8BU8GU8RU8:
		case FERMI_A_ZBC_COLOR_V0_FMT_A2R10G10B10:
		case FERMI_A_ZBC_COLOR_V0_FMT_BF10GF11RF11:
			ret = nvc0_graph_zbc_color_get(priv, args->v0.format,
							     args->v0.ds,
							     args->v0.l2);
			if (ret >= 0) {
				args->v0.index = ret;
				return 0;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	return ret;
}

static int
nvc0_fermi_mthd_zbc_depth(struct nouveau_object *object, void *data, u32 size)
{
	struct nvc0_graph_priv *priv = (void *)object->engine;
	union {
		struct fermi_a_zbc_depth_v0 v0;
	} *args = data;
	int ret;

	if (nvif_unpack(args->v0, 0, 0, false)) {
		switch (args->v0.format) {
		case FERMI_A_ZBC_DEPTH_V0_FMT_FP32:
			ret = nvc0_graph_zbc_depth_get(priv, args->v0.format,
							     args->v0.ds,
							     args->v0.l2);
			return (ret >= 0) ? 0 : -ENOSPC;
		default:
			return -EINVAL;
		}
	}

	return ret;
}

static int
nvc0_fermi_mthd(struct nouveau_object *object, u32 mthd, void *data, u32 size)
{
	switch (mthd) {
	case FERMI_A_ZBC_COLOR:
		return nvc0_fermi_mthd_zbc_color(object, data, size);
	case FERMI_A_ZBC_DEPTH:
		return nvc0_fermi_mthd_zbc_depth(object, data, size);
	default:
		break;
	}
	return -EINVAL;
}

struct nouveau_ofuncs
nvc0_fermi_ofuncs = {
	.ctor = _nouveau_object_ctor,
	.dtor = nouveau_object_destroy,
	.init = nouveau_object_init,
	.fini = nouveau_object_fini,
	.mthd = nvc0_fermi_mthd,
};

static int
nvc0_graph_set_shader_exceptions(struct nouveau_object *object, u32 mthd,
				 void *pdata, u32 size)
{
	struct nvc0_graph_priv *priv = (void *)nv_engine(object);
	if (size >= sizeof(u32)) {
		u32 data = *(u32 *)pdata ? 0xffffffff : 0x00000000;
		nv_wr32(priv, 0x419e44, data);
		nv_wr32(priv, 0x419e4c, data);
		return 0;
	}
	return -EINVAL;
}

struct nouveau_omthds
nvc0_graph_9097_omthds[] = {
	{ 0x1528, 0x1528, nvc0_graph_set_shader_exceptions },
	{}
};

struct nouveau_omthds
nvc0_graph_90c0_omthds[] = {
	{ 0x1528, 0x1528, nvc0_graph_set_shader_exceptions },
	{}
};

struct nouveau_oclass
nvc0_graph_sclass[] = {
	{ 0x902d, &nouveau_object_ofuncs },
	{ 0x9039, &nouveau_object_ofuncs },
	{ FERMI_A, &nvc0_fermi_ofuncs, nvc0_graph_9097_omthds },
	{ FERMI_COMPUTE_A, &nouveau_object_ofuncs, nvc0_graph_90c0_omthds },
	{}
};

/*******************************************************************************
 * PGRAPH context
 ******************************************************************************/

int
nvc0_graph_context_ctor(struct nouveau_object *parent,
			struct nouveau_object *engine,
			struct nouveau_oclass *oclass, void *args, u32 size,
			struct nouveau_object **pobject)
{
	struct nouveau_vm *vm = nouveau_client(parent)->vm;
	struct nvc0_graph_priv *priv = (void *)engine;
	struct nvc0_graph_data *data = priv->mmio_data;
	struct nvc0_graph_mmio *mmio = priv->mmio_list;
	struct nvc0_graph_chan *chan;
	int ret, i;

	/* allocate memory for context, and fill with default values */
	ret = nouveau_graph_context_create(parent, engine, oclass, NULL,
					   priv->size, 0x100,
					   NVOBJ_FLAG_ZERO_ALLOC, &chan);
	*pobject = nv_object(chan);
	if (ret)
		return ret;

	/* allocate memory for a "mmio list" buffer that's used by the HUB
	 * fuc to modify some per-context register settings on first load
	 * of the context.
	 */
	ret = nouveau_gpuobj_new(nv_object(chan), NULL, 0x1000, 0x100, 0,
				&chan->mmio);
	if (ret)
		return ret;

	ret = nouveau_gpuobj_map_vm(nv_gpuobj(chan->mmio), vm,
				    NV_MEM_ACCESS_RW | NV_MEM_ACCESS_SYS,
				    &chan->mmio_vma);
	if (ret)
		return ret;

	/* allocate buffers referenced by mmio list */
	for (i = 0; data->size && i < ARRAY_SIZE(priv->mmio_data); i++) {
		ret = nouveau_gpuobj_new(nv_object(chan), NULL, data->size,
					 data->align, 0, &chan->data[i].mem);
		if (ret)
			return ret;

		ret = nouveau_gpuobj_map_vm(chan->data[i].mem, vm, data->access,
					   &chan->data[i].vma);
		if (ret)
			return ret;

		data++;
	}

	/* finally, fill in the mmio list and point the context at it */
	for (i = 0; mmio->addr && i < ARRAY_SIZE(priv->mmio_list); i++) {
		u32 addr = mmio->addr;
		u32 data = mmio->data;

		if (mmio->buffer >= 0) {
			u64 info = chan->data[mmio->buffer].vma.offset;
			data |= info >> mmio->shift;
		}

		nv_wo32(chan->mmio, chan->mmio_nr++ * 4, addr);
		nv_wo32(chan->mmio, chan->mmio_nr++ * 4, data);
		mmio++;
	}

	for (i = 0; i < priv->size; i += 4)
		nv_wo32(chan, i, priv->data[i / 4]);

	if (!priv->firmware) {
		nv_wo32(chan, 0x00, chan->mmio_nr / 2);
		nv_wo32(chan, 0x04, chan->mmio_vma.offset >> 8);
	} else {
		nv_wo32(chan, 0xf4, 0);
		nv_wo32(chan, 0xf8, 0);
		nv_wo32(chan, 0x10, chan->mmio_nr / 2);
		nv_wo32(chan, 0x14, lower_32_bits(chan->mmio_vma.offset));
		nv_wo32(chan, 0x18, upper_32_bits(chan->mmio_vma.offset));
		nv_wo32(chan, 0x1c, 1);
		nv_wo32(chan, 0x20, 0);
		nv_wo32(chan, 0x28, 0);
		nv_wo32(chan, 0x2c, 0);
	}

	return 0;
}

void
nvc0_graph_context_dtor(struct nouveau_object *object)
{
	struct nvc0_graph_chan *chan = (void *)object;
	int i;

	for (i = 0; i < ARRAY_SIZE(chan->data); i++) {
		nouveau_gpuobj_unmap(&chan->data[i].vma);
		nouveau_gpuobj_ref(NULL, &chan->data[i].mem);
	}

	nouveau_gpuobj_unmap(&chan->mmio_vma);
	nouveau_gpuobj_ref(NULL, &chan->mmio);

	nouveau_graph_context_destroy(&chan->base);
}

/*******************************************************************************
 * PGRAPH register lists
 ******************************************************************************/

const struct nvc0_graph_init
nvc0_graph_init_main_0[] = {
	{ 0x400080,   1, 0x04, 0x003083c2 },
	{ 0x400088,   1, 0x04, 0x00006fe7 },
	{ 0x40008c,   1, 0x04, 0x00000000 },
	{ 0x400090,   1, 0x04, 0x00000030 },
	{ 0x40013c,   1, 0x04, 0x013901f7 },
	{ 0x400140,   1, 0x04, 0x00000100 },
	{ 0x400144,   1, 0x04, 0x00000000 },
	{ 0x400148,   1, 0x04, 0x00000110 },
	{ 0x400138,   1, 0x04, 0x00000000 },
	{ 0x400130,   2, 0x04, 0x00000000 },
	{ 0x400124,   1, 0x04, 0x00000002 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_fe_0[] = {
	{ 0x40415c,   1, 0x04, 0x00000000 },
	{ 0x404170,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_pri_0[] = {
	{ 0x404488,   2, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_rstr2d_0[] = {
	{ 0x407808,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_pd_0[] = {
	{ 0x406024,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_ds_0[] = {
	{ 0x405844,   1, 0x04, 0x00ffffff },
	{ 0x405850,   1, 0x04, 0x00000000 },
	{ 0x405908,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_scc_0[] = {
	{ 0x40803c,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_prop_0[] = {
	{ 0x4184a0,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_gpc_unk_0[] = {
	{ 0x418604,   1, 0x04, 0x00000000 },
	{ 0x418680,   1, 0x04, 0x00000000 },
	{ 0x418714,   1, 0x04, 0x80000000 },
	{ 0x418384,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_setup_0[] = {
	{ 0x418814,   3, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_crstr_0[] = {
	{ 0x418b04,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_setup_1[] = {
	{ 0x4188c8,   1, 0x04, 0x80000000 },
	{ 0x4188cc,   1, 0x04, 0x00000000 },
	{ 0x4188d0,   1, 0x04, 0x00010000 },
	{ 0x4188d4,   1, 0x04, 0x00000001 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_zcull_0[] = {
	{ 0x418910,   1, 0x04, 0x00010001 },
	{ 0x418914,   1, 0x04, 0x00000301 },
	{ 0x418918,   1, 0x04, 0x00800000 },
	{ 0x418980,   1, 0x04, 0x77777770 },
	{ 0x418984,   3, 0x04, 0x77777777 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_gpm_0[] = {
	{ 0x418c04,   1, 0x04, 0x00000000 },
	{ 0x418c88,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_gpc_unk_1[] = {
	{ 0x418d00,   1, 0x04, 0x00000000 },
	{ 0x418f08,   1, 0x04, 0x00000000 },
	{ 0x418e00,   1, 0x04, 0x00000050 },
	{ 0x418e08,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_gcc_0[] = {
	{ 0x41900c,   1, 0x04, 0x00000000 },
	{ 0x419018,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_tpccs_0[] = {
	{ 0x419d08,   2, 0x04, 0x00000000 },
	{ 0x419d10,   1, 0x04, 0x00000014 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_tex_0[] = {
	{ 0x419ab0,   1, 0x04, 0x00000000 },
	{ 0x419ab8,   1, 0x04, 0x000000e7 },
	{ 0x419abc,   2, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_pe_0[] = {
	{ 0x41980c,   3, 0x04, 0x00000000 },
	{ 0x419844,   1, 0x04, 0x00000000 },
	{ 0x41984c,   1, 0x04, 0x00005bc5 },
	{ 0x419850,   4, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_l1c_0[] = {
	{ 0x419c98,   1, 0x04, 0x00000000 },
	{ 0x419ca8,   1, 0x04, 0x80000000 },
	{ 0x419cb4,   1, 0x04, 0x00000000 },
	{ 0x419cb8,   1, 0x04, 0x00008bf4 },
	{ 0x419cbc,   1, 0x04, 0x28137606 },
	{ 0x419cc0,   2, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_wwdx_0[] = {
	{ 0x419bd4,   1, 0x04, 0x00800000 },
	{ 0x419bdc,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_tpccs_1[] = {
	{ 0x419d2c,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_mpc_0[] = {
	{ 0x419c0c,   1, 0x04, 0x00000000 },
	{}
};

static const struct nvc0_graph_init
nvc0_graph_init_sm_0[] = {
	{ 0x419e00,   1, 0x04, 0x00000000 },
	{ 0x419ea0,   1, 0x04, 0x00000000 },
	{ 0x419ea4,   1, 0x04, 0x00000100 },
	{ 0x419ea8,   1, 0x04, 0x00001100 },
	{ 0x419eac,   1, 0x04, 0x11100702 },
	{ 0x419eb0,   1, 0x04, 0x00000003 },
	{ 0x419eb4,   4, 0x04, 0x00000000 },
	{ 0x419ec8,   1, 0x04, 0x06060618 },
	{ 0x419ed0,   1, 0x04, 0x0eff0e38 },
	{ 0x419ed4,   1, 0x04, 0x011104f1 },
	{ 0x419edc,   1, 0x04, 0x00000000 },
	{ 0x419f00,   1, 0x04, 0x00000000 },
	{ 0x419f2c,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_be_0[] = {
	{ 0x40880c,   1, 0x04, 0x00000000 },
	{ 0x408910,   9, 0x04, 0x00000000 },
	{ 0x408950,   1, 0x04, 0x00000000 },
	{ 0x408954,   1, 0x04, 0x0000ffff },
	{ 0x408984,   1, 0x04, 0x00000000 },
	{ 0x408988,   1, 0x04, 0x08040201 },
	{ 0x40898c,   1, 0x04, 0x80402010 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_fe_1[] = {
	{ 0x4040f0,   1, 0x04, 0x00000000 },
	{}
};

const struct nvc0_graph_init
nvc0_graph_init_pe_1[] = {
	{ 0x419880,   1, 0x04, 0x00000002 },
	{}
};

static const struct nvc0_graph_pack
nvc0_graph_pack_mmio[] = {
	{ nvc0_graph_init_main_0 },
	{ nvc0_graph_init_fe_0 },
	{ nvc0_graph_init_pri_0 },
	{ nvc0_graph_init_rstr2d_0 },
	{ nvc0_graph_init_pd_0 },
	{ nvc0_graph_init_ds_0 },
	{ nvc0_graph_init_scc_0 },
	{ nvc0_graph_init_prop_0 },
	{ nvc0_graph_init_gpc_unk_0 },
	{ nvc0_graph_init_setup_0 },
	{ nvc0_graph_init_crstr_0 },
	{ nvc0_graph_init_setup_1 },
	{ nvc0_graph_init_zcull_0 },
	{ nvc0_graph_init_gpm_0 },
	{ nvc0_graph_init_gpc_unk_1 },
	{ nvc0_graph_init_gcc_0 },
	{ nvc0_graph_init_tpccs_0 },
	{ nvc0_graph_init_tex_0 },
	{ nvc0_graph_init_pe_0 },
	{ nvc0_graph_init_l1c_0 },
	{ nvc0_graph_init_wwdx_0 },
	{ nvc0_graph_init_tpccs_1 },
	{ nvc0_graph_init_mpc_0 },
	{ nvc0_graph_init_sm_0 },
	{ nvc0_graph_init_be_0 },
	{ nvc0_graph_init_fe_1 },
	{ nvc0_graph_init_pe_1 },
	{}
};

/*******************************************************************************
 * PGRAPH engine/subdev functions
 ******************************************************************************/

void
nvc0_graph_zbc_init(struct nvc0_graph_priv *priv)
{
	const u32  zero[] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
			      0x00000000, 0x00000000, 0x00000000, 0x00000000 };
	const u32   one[] = { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000,
			      0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
	const u32 f32_0[] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
			      0x00000000, 0x00000000, 0x00000000, 0x00000000 };
	const u32 f32_1[] = { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000,
			      0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 };
	struct nouveau_ltc *ltc = nouveau_ltc(priv);
	int index;

	if (!priv->zbc_color[0].format) {
		nvc0_graph_zbc_color_get(priv, 1,  & zero[0],   &zero[4]);
		nvc0_graph_zbc_color_get(priv, 2,  &  one[0],    &one[4]);
		nvc0_graph_zbc_color_get(priv, 4,  &f32_0[0],  &f32_0[4]);
		nvc0_graph_zbc_color_get(priv, 4,  &f32_1[0],  &f32_1[4]);
		nvc0_graph_zbc_depth_get(priv, 1, 0x00000000, 0x00000000);
		nvc0_graph_zbc_depth_get(priv, 1, 0x3f800000, 0x3f800000);
	}

	for (index = ltc->zbc_min; index <= ltc->zbc_max; index++)
		nvc0_graph_zbc_clear_color(priv, index);
	for (index = ltc->zbc_min; index <= ltc->zbc_max; index++)
		nvc0_graph_zbc_clear_depth(priv, index);
}

void
nvc0_graph_mmio(struct nvc0_graph_priv *priv, const struct nvc0_graph_pack *p)
{
	const struct nvc0_graph_pack *pack;
	const struct nvc0_graph_init *init;

	pack_for_each_init(init, pack, p) {
		u32 next = init->addr + init->count * init->pitch;
		u32 addr = init->addr;
		while (addr < next) {
			nv_wr32(priv, addr, init->data);
			addr += init->pitch;
		}
	}
}

void
nvc0_graph_icmd(struct nvc0_graph_priv *priv, const struct nvc0_graph_pack *p)
{
	const struct nvc0_graph_pack *pack;
	const struct nvc0_graph_init *init;
	u32 data = 0;

	nv_wr32(priv, 0x400208, 0x80000000);

	pack_for_each_init(init, pack, p) {
		u32 next = init->addr + init->count * init->pitch;
		u32 addr = init->addr;

		if ((pack == p && init == p->init) || data != init->data) {
			nv_wr32(priv, 0x400204, init->data);
			data = init->data;
		}

		while (addr < next) {
			nv_wr32(priv, 0x400200, addr);
			nv_wait(priv, 0x400700, 0x00000002, 0x00000000);
			addr += init->pitch;
		}
	}

	nv_wr32(priv, 0x400208, 0x00000000);
}

void
nvc0_graph_mthd(struct nvc0_graph_priv *priv, const struct nvc0_graph_pack *p)
{
	const struct nvc0_graph_pack *pack;
	const struct nvc0_graph_init *init;
	u32 data = 0;

	pack_for_each_init(init, pack, p) {
		u32 ctrl = 0x80000000 | pack->type;
		u32 next = init->addr + init->count * init->pitch;
		u32 addr = init->addr;

		if ((pack == p && init == p->init) || data != init->data) {
			nv_wr32(priv, 0x40448c, init->data);
			data = init->data;
		}

		while (addr < next) {
			nv_wr32(priv, 0x404488, ctrl | (addr << 14));
			addr += init->pitch;
		}
	}
}

u64
nvc0_graph_units(struct nouveau_graph *graph)
{
	struct nvc0_graph_priv *priv = (void *)graph;
	u64 cfg;

	cfg  = (u32)priv->gpc_nr;
	cfg |= (u32)priv->tpc_total << 8;
	cfg |= (u64)priv->rop_nr << 32;

	return cfg;
}

static const struct nouveau_enum nve0_sked_error[] = {
	{ 7, "CONSTANT_BUFFER_SIZE" },
	{ 9, "LOCAL_MEMORY_SIZE_POS" },
	{ 10, "LOCAL_MEMORY_SIZE_NEG" },
	{ 11, "WARP_CSTACK_SIZE" },
	{ 12, "TOTAL_TEMP_SIZE" },
	{ 13, "REGISTER_COUNT" },
	{ 18, "TOTAL_THREADS" },
	{ 20, "PROGRAM_OFFSET" },
	{ 21, "SHARED_MEMORY_SIZE" },
	{ 25, "SHARED_CONFIG_TOO_SMALL" },
	{ 26, "TOTAL_REGISTER_COUNT" },
	{}
};

static const struct nouveau_enum nvc0_gpc_rop_error[] = {
	{ 1, "RT_PITCH_OVERRUN" },
	{ 4, "RT_WIDTH_OVERRUN" },
	{ 5, "RT_HEIGHT_OVERRUN" },
	{ 7, "ZETA_STORAGE_TYPE_MISMATCH" },
	{ 8, "RT_STORAGE_TYPE_MISMATCH" },
	{ 10, "RT_LINEAR_MISMATCH" },
	{}
};

static void
nvc0_graph_trap_gpc_rop(struct nvc0_graph_priv *priv, int gpc)
{
	u32 trap[4];
	int i;

	trap[0] = nv_rd32(priv, GPC_UNIT(gpc, 0x0420));
	trap[1] = nv_rd32(priv, GPC_UNIT(gpc, 0x0434));
	trap[2] = nv_rd32(priv, GPC_UNIT(gpc, 0x0438));
	trap[3] = nv_rd32(priv, GPC_UNIT(gpc, 0x043c));

	nv_error(priv, "GPC%d/PROP trap:", gpc);
	for (i = 0; i <= 29; ++i) {
		if (!(trap[0] & (1 << i)))
			continue;
		pr_cont(" ");
		nouveau_enum_print(nvc0_gpc_rop_error, i);
	}
	pr_cont("\n");

	nv_error(priv, "x = %u, y = %u, format = %x, storage type = %x\n",
		 trap[1] & 0xffff, trap[1] >> 16, (trap[2] >> 8) & 0x3f,
		 trap[3] & 0xff);
	nv_wr32(priv, GPC_UNIT(gpc, 0x0420), 0xc0000000);
}

static const struct nouveau_enum nvc0_mp_warp_error[] = {
	{ 0x00, "NO_ERROR" },
	{ 0x01, "STACK_MISMATCH" },
	{ 0x05, "MISALIGNED_PC" },
	{ 0x08, "MISALIGNED_GPR" },
	{ 0x09, "INVALID_OPCODE" },
	{ 0x0d, "GPR_OUT_OF_BOUNDS" },
	{ 0x0e, "MEM_OUT_OF_BOUNDS" },
	{ 0x0f, "UNALIGNED_MEM_ACCESS" },
	{ 0x11, "INVALID_PARAM" },
	{}
};

static const struct nouveau_bitfield nvc0_mp_global_error[] = {
	{ 0x00000004, "MULTIPLE_WARP_ERRORS" },
	{ 0x00000008, "OUT_OF_STACK_SPACE" },
	{}
};

static void
nvc0_graph_trap_mp(struct nvc0_graph_priv *priv, int gpc, int tpc)
{
	u32 werr = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x648));
	u32 gerr = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x650));

	nv_error(priv, "GPC%i/TPC%i/MP trap:", gpc, tpc);
	nouveau_bitfield_print(nvc0_mp_global_error, gerr);
	if (werr) {
		pr_cont(" ");
		nouveau_enum_print(nvc0_mp_warp_error, werr & 0xffff);
	}
	pr_cont("\n");

	nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x648), 0x00000000);
	nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x650), gerr);
}

static void
nvc0_graph_trap_tpc(struct nvc0_graph_priv *priv, int gpc, int tpc)
{
	u32 stat = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x0508));

	if (stat & 0x00000001) {
		u32 trap = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x0224));
		nv_error(priv, "GPC%d/TPC%d/TEX: 0x%08x\n", gpc, tpc, trap);
		nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x0224), 0xc0000000);
		stat &= ~0x00000001;
	}

	if (stat & 0x00000002) {
		nvc0_graph_trap_mp(priv, gpc, tpc);
		stat &= ~0x00000002;
	}

	if (stat & 0x00000004) {
		u32 trap = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x0084));
		nv_error(priv, "GPC%d/TPC%d/POLY: 0x%08x\n", gpc, tpc, trap);
		nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x0084), 0xc0000000);
		stat &= ~0x00000004;
	}

	if (stat & 0x00000008) {
		u32 trap = nv_rd32(priv, TPC_UNIT(gpc, tpc, 0x048c));
		nv_error(priv, "GPC%d/TPC%d/L1C: 0x%08x\n", gpc, tpc, trap);
		nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x048c), 0xc0000000);
		stat &= ~0x00000008;
	}

	if (stat) {
		nv_error(priv, "GPC%d/TPC%d/0x%08x: unknown\n", gpc, tpc, stat);
	}
}

static void
nvc0_graph_trap_gpc(struct nvc0_graph_priv *priv, int gpc)
{
	u32 stat = nv_rd32(priv, GPC_UNIT(gpc, 0x2c90));
	int tpc;

	if (stat & 0x00000001) {
		nvc0_graph_trap_gpc_rop(priv, gpc);
		stat &= ~0x00000001;
	}

	if (stat & 0x00000002) {
		u32 trap = nv_rd32(priv, GPC_UNIT(gpc, 0x0900));
		nv_error(priv, "GPC%d/ZCULL: 0x%08x\n", gpc, trap);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0900), 0xc0000000);
		stat &= ~0x00000002;
	}

	if (stat & 0x00000004) {
		u32 trap = nv_rd32(priv, GPC_UNIT(gpc, 0x1028));
		nv_error(priv, "GPC%d/CCACHE: 0x%08x\n", gpc, trap);
		nv_wr32(priv, GPC_UNIT(gpc, 0x1028), 0xc0000000);
		stat &= ~0x00000004;
	}

	if (stat & 0x00000008) {
		u32 trap = nv_rd32(priv, GPC_UNIT(gpc, 0x0824));
		nv_error(priv, "GPC%d/ESETUP: 0x%08x\n", gpc, trap);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0824), 0xc0000000);
		stat &= ~0x00000009;
	}

	for (tpc = 0; tpc < priv->tpc_nr[gpc]; tpc++) {
		u32 mask = 0x00010000 << tpc;
		if (stat & mask) {
			nvc0_graph_trap_tpc(priv, gpc, tpc);
			nv_wr32(priv, GPC_UNIT(gpc, 0x2c90), mask);
			stat &= ~mask;
		}
	}

	if (stat) {
		nv_error(priv, "GPC%d/0x%08x: unknown\n", gpc, stat);
	}
}

static void
nvc0_graph_trap_intr(struct nvc0_graph_priv *priv)
{
	u32 trap = nv_rd32(priv, 0x400108);
	int rop, gpc, i;

	if (trap & 0x00000001) {
		u32 stat = nv_rd32(priv, 0x404000);
		nv_error(priv, "DISPATCH 0x%08x\n", stat);
		nv_wr32(priv, 0x404000, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000001);
		trap &= ~0x00000001;
	}

	if (trap & 0x00000002) {
		u32 stat = nv_rd32(priv, 0x404600);
		nv_error(priv, "M2MF 0x%08x\n", stat);
		nv_wr32(priv, 0x404600, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000002);
		trap &= ~0x00000002;
	}

	if (trap & 0x00000008) {
		u32 stat = nv_rd32(priv, 0x408030);
		nv_error(priv, "CCACHE 0x%08x\n", stat);
		nv_wr32(priv, 0x408030, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000008);
		trap &= ~0x00000008;
	}

	if (trap & 0x00000010) {
		u32 stat = nv_rd32(priv, 0x405840);
		nv_error(priv, "SHADER 0x%08x\n", stat);
		nv_wr32(priv, 0x405840, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000010);
		trap &= ~0x00000010;
	}

	if (trap & 0x00000040) {
		u32 stat = nv_rd32(priv, 0x40601c);
		nv_error(priv, "UNK6 0x%08x\n", stat);
		nv_wr32(priv, 0x40601c, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000040);
		trap &= ~0x00000040;
	}

	if (trap & 0x00000080) {
		u32 stat = nv_rd32(priv, 0x404490);
		nv_error(priv, "MACRO 0x%08x\n", stat);
		nv_wr32(priv, 0x404490, 0xc0000000);
		nv_wr32(priv, 0x400108, 0x00000080);
		trap &= ~0x00000080;
	}

	if (trap & 0x00000100) {
		u32 stat = nv_rd32(priv, 0x407020);

		nv_error(priv, "SKED:");
		for (i = 0; i <= 29; ++i) {
			if (!(stat & (1 << i)))
				continue;
			pr_cont(" ");
			nouveau_enum_print(nve0_sked_error, i);
		}
		pr_cont("\n");

		if (stat & 0x3fffffff)
			nv_wr32(priv, 0x407020, 0x40000000);
		nv_wr32(priv, 0x400108, 0x00000100);
		trap &= ~0x00000100;
	}

	if (trap & 0x01000000) {
		u32 stat = nv_rd32(priv, 0x400118);
		for (gpc = 0; stat && gpc < priv->gpc_nr; gpc++) {
			u32 mask = 0x00000001 << gpc;
			if (stat & mask) {
				nvc0_graph_trap_gpc(priv, gpc);
				nv_wr32(priv, 0x400118, mask);
				stat &= ~mask;
			}
		}
		nv_wr32(priv, 0x400108, 0x01000000);
		trap &= ~0x01000000;
	}

	if (trap & 0x02000000) {
		for (rop = 0; rop < priv->rop_nr; rop++) {
			u32 statz = nv_rd32(priv, ROP_UNIT(rop, 0x070));
			u32 statc = nv_rd32(priv, ROP_UNIT(rop, 0x144));
			nv_error(priv, "ROP%d 0x%08x 0x%08x\n",
				 rop, statz, statc);
			nv_wr32(priv, ROP_UNIT(rop, 0x070), 0xc0000000);
			nv_wr32(priv, ROP_UNIT(rop, 0x144), 0xc0000000);
		}
		nv_wr32(priv, 0x400108, 0x02000000);
		trap &= ~0x02000000;
	}

	if (trap) {
		nv_error(priv, "TRAP UNHANDLED 0x%08x\n", trap);
		nv_wr32(priv, 0x400108, trap);
	}
}

static void
nvc0_graph_ctxctl_debug_unit(struct nvc0_graph_priv *priv, u32 base)
{
	nv_error(priv, "%06x - done 0x%08x\n", base,
		 nv_rd32(priv, base + 0x400));
	nv_error(priv, "%06x - stat 0x%08x 0x%08x 0x%08x 0x%08x\n", base,
		 nv_rd32(priv, base + 0x800), nv_rd32(priv, base + 0x804),
		 nv_rd32(priv, base + 0x808), nv_rd32(priv, base + 0x80c));
	nv_error(priv, "%06x - stat 0x%08x 0x%08x 0x%08x 0x%08x\n", base,
		 nv_rd32(priv, base + 0x810), nv_rd32(priv, base + 0x814),
		 nv_rd32(priv, base + 0x818), nv_rd32(priv, base + 0x81c));
}

void
nvc0_graph_ctxctl_debug(struct nvc0_graph_priv *priv)
{
	u32 gpcnr = nv_rd32(priv, 0x409604) & 0xffff;
	u32 gpc;

	nvc0_graph_ctxctl_debug_unit(priv, 0x409000);
	for (gpc = 0; gpc < gpcnr; gpc++)
		nvc0_graph_ctxctl_debug_unit(priv, 0x502000 + (gpc * 0x8000));
}

static void
nvc0_graph_ctxctl_isr(struct nvc0_graph_priv *priv)
{
	u32 stat = nv_rd32(priv, 0x409c18);

	if (stat & 0x00000001) {
		u32 code = nv_rd32(priv, 0x409814);
		if (code == E_BAD_FWMTHD) {
			u32 class = nv_rd32(priv, 0x409808);
			u32  addr = nv_rd32(priv, 0x40980c);
			u32  subc = (addr & 0x00070000) >> 16;
			u32  mthd = (addr & 0x00003ffc);
			u32  data = nv_rd32(priv, 0x409810);

			nv_error(priv, "FECS MTHD subc %d class 0x%04x "
				       "mthd 0x%04x data 0x%08x\n",
				 subc, class, mthd, data);

			nv_wr32(priv, 0x409c20, 0x00000001);
			stat &= ~0x00000001;
		} else {
			nv_error(priv, "FECS ucode error %d\n", code);
		}
	}

	if (stat & 0x00080000) {
		nv_error(priv, "FECS watchdog timeout\n");
		nvc0_graph_ctxctl_debug(priv);
		nv_wr32(priv, 0x409c20, 0x00080000);
		stat &= ~0x00080000;
	}

	if (stat) {
		nv_error(priv, "FECS 0x%08x\n", stat);
		nvc0_graph_ctxctl_debug(priv);
		nv_wr32(priv, 0x409c20, stat);
	}
}

static void
nvc0_graph_intr(struct nouveau_subdev *subdev)
{
	struct nouveau_fifo *pfifo = nouveau_fifo(subdev);
	struct nouveau_engine *engine = nv_engine(subdev);
	struct nouveau_object *engctx;
	struct nouveau_handle *handle;
	struct nvc0_graph_priv *priv = (void *)subdev;
	u64 inst = nv_rd32(priv, 0x409b00) & 0x0fffffff;
	u32 stat = nv_rd32(priv, 0x400100);
	u32 addr = nv_rd32(priv, 0x400704);
	u32 mthd = (addr & 0x00003ffc);
	u32 subc = (addr & 0x00070000) >> 16;
	u32 data = nv_rd32(priv, 0x400708);
	u32 code = nv_rd32(priv, 0x400110);
	u32 class = nv_rd32(priv, 0x404200 + (subc * 4));
	int chid;

	engctx = nouveau_engctx_get(engine, inst);
	chid   = pfifo->chid(pfifo, engctx);

	if (stat & 0x00000010) {
		handle = nouveau_handle_get_class(engctx, class);
		if (!handle || nv_call(handle->object, mthd, data)) {
			nv_error(priv,
				 "ILLEGAL_MTHD ch %d [0x%010llx %s] subc %d class 0x%04x mthd 0x%04x data 0x%08x\n",
				 chid, inst << 12, nouveau_client_name(engctx),
				 subc, class, mthd, data);
		}
		nouveau_handle_put(handle);
		nv_wr32(priv, 0x400100, 0x00000010);
		stat &= ~0x00000010;
	}

	if (stat & 0x00000020) {
		nv_error(priv,
			 "ILLEGAL_CLASS ch %d [0x%010llx %s] subc %d class 0x%04x mthd 0x%04x data 0x%08x\n",
			 chid, inst << 12, nouveau_client_name(engctx), subc,
			 class, mthd, data);
		nv_wr32(priv, 0x400100, 0x00000020);
		stat &= ~0x00000020;
	}

	if (stat & 0x00100000) {
		nv_error(priv, "DATA_ERROR [");
		nouveau_enum_print(nv50_data_error_names, code);
		pr_cont("] ch %d [0x%010llx %s] subc %d class 0x%04x mthd 0x%04x data 0x%08x\n",
			chid, inst << 12, nouveau_client_name(engctx), subc,
			class, mthd, data);
		nv_wr32(priv, 0x400100, 0x00100000);
		stat &= ~0x00100000;
	}

	if (stat & 0x00200000) {
		nv_error(priv, "TRAP ch %d [0x%010llx %s]\n", chid, inst << 12,
			 nouveau_client_name(engctx));
		nvc0_graph_trap_intr(priv);
		nv_wr32(priv, 0x400100, 0x00200000);
		stat &= ~0x00200000;
	}

	if (stat & 0x00080000) {
		nvc0_graph_ctxctl_isr(priv);
		nv_wr32(priv, 0x400100, 0x00080000);
		stat &= ~0x00080000;
	}

	if (stat) {
		nv_error(priv, "unknown stat 0x%08x\n", stat);
		nv_wr32(priv, 0x400100, stat);
	}

	nv_wr32(priv, 0x400500, 0x00010001);
	nouveau_engctx_put(engctx);
}

void
nvc0_graph_init_fw(struct nvc0_graph_priv *priv, u32 fuc_base,
		   struct nvc0_graph_fuc *code, struct nvc0_graph_fuc *data)
{
	int i;

	nv_wr32(priv, fuc_base + 0x01c0, 0x01000000);
	for (i = 0; i < data->size / 4; i++)
		nv_wr32(priv, fuc_base + 0x01c4, data->data[i]);

	nv_wr32(priv, fuc_base + 0x0180, 0x01000000);
	for (i = 0; i < code->size / 4; i++) {
		if ((i & 0x3f) == 0)
			nv_wr32(priv, fuc_base + 0x0188, i >> 6);
		nv_wr32(priv, fuc_base + 0x0184, code->data[i]);
	}

	/* code must be padded to 0x40 words */
	for (; i & 0x3f; i++)
		nv_wr32(priv, fuc_base + 0x0184, 0);
}

static void
nvc0_graph_init_csdata(struct nvc0_graph_priv *priv,
		       const struct nvc0_graph_pack *pack,
		       u32 falcon, u32 starstar, u32 base)
{
	const struct nvc0_graph_pack *iter;
	const struct nvc0_graph_init *init;
	u32 addr = ~0, prev = ~0, xfer = 0;
	u32 star, temp;

	nv_wr32(priv, falcon + 0x01c0, 0x02000000 + starstar);
	star = nv_rd32(priv, falcon + 0x01c4);
	temp = nv_rd32(priv, falcon + 0x01c4);
	if (temp > star)
		star = temp;
	nv_wr32(priv, falcon + 0x01c0, 0x01000000 + star);

	pack_for_each_init(init, iter, pack) {
		u32 head = init->addr - base;
		u32 tail = head + init->count * init->pitch;
		while (head < tail) {
			if (head != prev + 4 || xfer >= 32) {
				if (xfer) {
					u32 data = ((--xfer << 26) | addr);
					nv_wr32(priv, falcon + 0x01c4, data);
					star += 4;
				}
				addr = head;
				xfer = 0;
			}
			prev = head;
			xfer = xfer + 1;
			head = head + init->pitch;
		}
	}

	nv_wr32(priv, falcon + 0x01c4, (--xfer << 26) | addr);
	nv_wr32(priv, falcon + 0x01c0, 0x01000004 + starstar);
	nv_wr32(priv, falcon + 0x01c4, star + 4);
}

int
nvc0_graph_init_ctxctl(struct nvc0_graph_priv *priv)
{
	struct nvc0_graph_oclass *oclass = (void *)nv_object(priv)->oclass;
	struct nvc0_grctx_oclass *cclass = (void *)nv_engine(priv)->cclass;
	int i;

	if (priv->firmware) {
		/* load fuc microcode */
		nouveau_mc(priv)->unk260(nouveau_mc(priv), 0);
		nvc0_graph_init_fw(priv, 0x409000, &priv->fuc409c,
						   &priv->fuc409d);
		nvc0_graph_init_fw(priv, 0x41a000, &priv->fuc41ac,
						   &priv->fuc41ad);
		nouveau_mc(priv)->unk260(nouveau_mc(priv), 1);

		/* start both of them running */
		nv_wr32(priv, 0x409840, 0xffffffff);
		nv_wr32(priv, 0x41a10c, 0x00000000);
		nv_wr32(priv, 0x40910c, 0x00000000);
		nv_wr32(priv, 0x41a100, 0x00000002);
		nv_wr32(priv, 0x409100, 0x00000002);
		if (!nv_wait(priv, 0x409800, 0x00000001, 0x00000001))
			nv_warn(priv, "0x409800 wait failed\n");

		nv_wr32(priv, 0x409840, 0xffffffff);
		nv_wr32(priv, 0x409500, 0x7fffffff);
		nv_wr32(priv, 0x409504, 0x00000021);

		nv_wr32(priv, 0x409840, 0xffffffff);
		nv_wr32(priv, 0x409500, 0x00000000);
		nv_wr32(priv, 0x409504, 0x00000010);
		if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
			nv_error(priv, "fuc09 req 0x10 timeout\n");
			return -EBUSY;
		}
		priv->size = nv_rd32(priv, 0x409800);

		nv_wr32(priv, 0x409840, 0xffffffff);
		nv_wr32(priv, 0x409500, 0x00000000);
		nv_wr32(priv, 0x409504, 0x00000016);
		if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
			nv_error(priv, "fuc09 req 0x16 timeout\n");
			return -EBUSY;
		}

		nv_wr32(priv, 0x409840, 0xffffffff);
		nv_wr32(priv, 0x409500, 0x00000000);
		nv_wr32(priv, 0x409504, 0x00000025);
		if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
			nv_error(priv, "fuc09 req 0x25 timeout\n");
			return -EBUSY;
		}

		if (nv_device(priv)->chipset >= 0xe0) {
			nv_wr32(priv, 0x409800, 0x00000000);
			nv_wr32(priv, 0x409500, 0x00000001);
			nv_wr32(priv, 0x409504, 0x00000030);
			if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
				nv_error(priv, "fuc09 req 0x30 timeout\n");
				return -EBUSY;
			}

			nv_wr32(priv, 0x409810, 0xb00095c8);
			nv_wr32(priv, 0x409800, 0x00000000);
			nv_wr32(priv, 0x409500, 0x00000001);
			nv_wr32(priv, 0x409504, 0x00000031);
			if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
				nv_error(priv, "fuc09 req 0x31 timeout\n");
				return -EBUSY;
			}

			nv_wr32(priv, 0x409810, 0x00080420);
			nv_wr32(priv, 0x409800, 0x00000000);
			nv_wr32(priv, 0x409500, 0x00000001);
			nv_wr32(priv, 0x409504, 0x00000032);
			if (!nv_wait_ne(priv, 0x409800, 0xffffffff, 0x00000000)) {
				nv_error(priv, "fuc09 req 0x32 timeout\n");
				return -EBUSY;
			}

			nv_wr32(priv, 0x409614, 0x00000070);
			nv_wr32(priv, 0x409614, 0x00000770);
			nv_wr32(priv, 0x40802c, 0x00000001);
		}

		if (priv->data == NULL) {
			int ret = nvc0_grctx_generate(priv);
			if (ret) {
				nv_error(priv, "failed to construct context\n");
				return ret;
			}
		}

		return 0;
	} else
	if (!oclass->fecs.ucode) {
		return -ENOSYS;
	}

	/* load HUB microcode */
	nouveau_mc(priv)->unk260(nouveau_mc(priv), 0);
	nv_wr32(priv, 0x4091c0, 0x01000000);
	for (i = 0; i < oclass->fecs.ucode->data.size / 4; i++)
		nv_wr32(priv, 0x4091c4, oclass->fecs.ucode->data.data[i]);

	nv_wr32(priv, 0x409180, 0x01000000);
	for (i = 0; i < oclass->fecs.ucode->code.size / 4; i++) {
		if ((i & 0x3f) == 0)
			nv_wr32(priv, 0x409188, i >> 6);
		nv_wr32(priv, 0x409184, oclass->fecs.ucode->code.data[i]);
	}

	/* load GPC microcode */
	nv_wr32(priv, 0x41a1c0, 0x01000000);
	for (i = 0; i < oclass->gpccs.ucode->data.size / 4; i++)
		nv_wr32(priv, 0x41a1c4, oclass->gpccs.ucode->data.data[i]);

	nv_wr32(priv, 0x41a180, 0x01000000);
	for (i = 0; i < oclass->gpccs.ucode->code.size / 4; i++) {
		if ((i & 0x3f) == 0)
			nv_wr32(priv, 0x41a188, i >> 6);
		nv_wr32(priv, 0x41a184, oclass->gpccs.ucode->code.data[i]);
	}
	nouveau_mc(priv)->unk260(nouveau_mc(priv), 1);

	/* load register lists */
	nvc0_graph_init_csdata(priv, cclass->hub, 0x409000, 0x000, 0x000000);
	nvc0_graph_init_csdata(priv, cclass->gpc, 0x41a000, 0x000, 0x418000);
	nvc0_graph_init_csdata(priv, cclass->tpc, 0x41a000, 0x004, 0x419800);
	nvc0_graph_init_csdata(priv, cclass->ppc, 0x41a000, 0x008, 0x41be00);

	/* start HUB ucode running, it'll init the GPCs */
	nv_wr32(priv, 0x40910c, 0x00000000);
	nv_wr32(priv, 0x409100, 0x00000002);
	if (!nv_wait(priv, 0x409800, 0x80000000, 0x80000000)) {
		nv_error(priv, "HUB_INIT timed out\n");
		nvc0_graph_ctxctl_debug(priv);
		return -EBUSY;
	}

	priv->size = nv_rd32(priv, 0x409804);
	if (priv->data == NULL) {
		int ret = nvc0_grctx_generate(priv);
		if (ret) {
			nv_error(priv, "failed to construct context\n");
			return ret;
		}
	}

	return 0;
}

int
nvc0_graph_init(struct nouveau_object *object)
{
	struct nvc0_graph_oclass *oclass = (void *)object->oclass;
	struct nvc0_graph_priv *priv = (void *)object;
	const u32 magicgpc918 = DIV_ROUND_UP(0x00800000, priv->tpc_total);
	u32 data[TPC_MAX / 8] = {};
	u8  tpcnr[GPC_MAX];
	int gpc, tpc, rop;
	int ret, i;

	ret = nouveau_graph_init(&priv->base);
	if (ret)
		return ret;

	nv_wr32(priv, GPC_BCAST(0x0880), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x08a4), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x0888), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x088c), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x0890), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x0894), 0x00000000);
	nv_wr32(priv, GPC_BCAST(0x08b4), priv->unk4188b4->addr >> 8);
	nv_wr32(priv, GPC_BCAST(0x08b8), priv->unk4188b8->addr >> 8);

	nvc0_graph_mmio(priv, oclass->mmio);

	memcpy(tpcnr, priv->tpc_nr, sizeof(priv->tpc_nr));
	for (i = 0, gpc = -1; i < priv->tpc_total; i++) {
		do {
			gpc = (gpc + 1) % priv->gpc_nr;
		} while (!tpcnr[gpc]);
		tpc = priv->tpc_nr[gpc] - tpcnr[gpc]--;

		data[i / 8] |= tpc << ((i % 8) * 4);
	}

	nv_wr32(priv, GPC_BCAST(0x0980), data[0]);
	nv_wr32(priv, GPC_BCAST(0x0984), data[1]);
	nv_wr32(priv, GPC_BCAST(0x0988), data[2]);
	nv_wr32(priv, GPC_BCAST(0x098c), data[3]);

	for (gpc = 0; gpc < priv->gpc_nr; gpc++) {
		nv_wr32(priv, GPC_UNIT(gpc, 0x0914),
			priv->magic_not_rop_nr << 8 | priv->tpc_nr[gpc]);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0910), 0x00040000 |
			priv->tpc_total);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0918), magicgpc918);
	}

	if (nv_device(priv)->chipset != 0xd7)
		nv_wr32(priv, GPC_BCAST(0x1bd4), magicgpc918);
	else
		nv_wr32(priv, GPC_BCAST(0x3fd4), magicgpc918);

	nv_wr32(priv, GPC_BCAST(0x08ac), nv_rd32(priv, 0x100800));

	nv_wr32(priv, 0x400500, 0x00010001);

	nv_wr32(priv, 0x400100, 0xffffffff);
	nv_wr32(priv, 0x40013c, 0xffffffff);

	nv_wr32(priv, 0x409c24, 0x000f0000);
	nv_wr32(priv, 0x404000, 0xc0000000);
	nv_wr32(priv, 0x404600, 0xc0000000);
	nv_wr32(priv, 0x408030, 0xc0000000);
	nv_wr32(priv, 0x40601c, 0xc0000000);
	nv_wr32(priv, 0x404490, 0xc0000000);
	nv_wr32(priv, 0x406018, 0xc0000000);
	nv_wr32(priv, 0x405840, 0xc0000000);
	nv_wr32(priv, 0x405844, 0x00ffffff);
	nv_mask(priv, 0x419cc0, 0x00000008, 0x00000008);
	nv_mask(priv, 0x419eb4, 0x00001000, 0x00001000);

	for (gpc = 0; gpc < priv->gpc_nr; gpc++) {
		nv_wr32(priv, GPC_UNIT(gpc, 0x0420), 0xc0000000);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0900), 0xc0000000);
		nv_wr32(priv, GPC_UNIT(gpc, 0x1028), 0xc0000000);
		nv_wr32(priv, GPC_UNIT(gpc, 0x0824), 0xc0000000);
		for (tpc = 0; tpc < priv->tpc_nr[gpc]; tpc++) {
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x508), 0xffffffff);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x50c), 0xffffffff);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x224), 0xc0000000);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x48c), 0xc0000000);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x084), 0xc0000000);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x644), 0x001ffffe);
			nv_wr32(priv, TPC_UNIT(gpc, tpc, 0x64c), 0x0000000f);
		}
		nv_wr32(priv, GPC_UNIT(gpc, 0x2c90), 0xffffffff);
		nv_wr32(priv, GPC_UNIT(gpc, 0x2c94), 0xffffffff);
	}

	for (rop = 0; rop < priv->rop_nr; rop++) {
		nv_wr32(priv, ROP_UNIT(rop, 0x144), 0xc0000000);
		nv_wr32(priv, ROP_UNIT(rop, 0x070), 0xc0000000);
		nv_wr32(priv, ROP_UNIT(rop, 0x204), 0xffffffff);
		nv_wr32(priv, ROP_UNIT(rop, 0x208), 0xffffffff);
	}

	nv_wr32(priv, 0x400108, 0xffffffff);
	nv_wr32(priv, 0x400138, 0xffffffff);
	nv_wr32(priv, 0x400118, 0xffffffff);
	nv_wr32(priv, 0x400130, 0xffffffff);
	nv_wr32(priv, 0x40011c, 0xffffffff);
	nv_wr32(priv, 0x400134, 0xffffffff);

	nv_wr32(priv, 0x400054, 0x34ce3464);

	nvc0_graph_zbc_init(priv);

	return nvc0_graph_init_ctxctl(priv);
}

static void
nvc0_graph_dtor_fw(struct nvc0_graph_fuc *fuc)
{
	kfree(fuc->data);
	fuc->data = NULL;
}

int
nvc0_graph_ctor_fw(struct nvc0_graph_priv *priv, const char *fwname,
		   struct nvc0_graph_fuc *fuc)
{
	struct nouveau_device *device = nv_device(priv);
	const struct firmware *fw;
	char f[32];
	int ret;

	snprintf(f, sizeof(f), "/*(DEBLOBBED)*/", device->chipset, fwname);
	ret = reject_firmware(&fw, f, nv_device_base(device));
	if (ret) {
		snprintf(f, sizeof(f), "/*(DEBLOBBED)*/", fwname);
		ret = reject_firmware(&fw, f, nv_device_base(device));
		if (ret) {
			nv_error(priv, "failed to load %s\n", fwname);
			return ret;
		}
	}

	fuc->size = fw->size;
	fuc->data = kmemdup(fw->data, fuc->size, GFP_KERNEL);
	release_firmware(fw);
	return (fuc->data != NULL) ? 0 : -ENOMEM;
}

void
nvc0_graph_dtor(struct nouveau_object *object)
{
	struct nvc0_graph_priv *priv = (void *)object;

	kfree(priv->data);

	nvc0_graph_dtor_fw(&priv->fuc409c);
	nvc0_graph_dtor_fw(&priv->fuc409d);
	nvc0_graph_dtor_fw(&priv->fuc41ac);
	nvc0_graph_dtor_fw(&priv->fuc41ad);

	nouveau_gpuobj_ref(NULL, &priv->unk4188b8);
	nouveau_gpuobj_ref(NULL, &priv->unk4188b4);

	nouveau_graph_destroy(&priv->base);
}

int
nvc0_graph_ctor(struct nouveau_object *parent, struct nouveau_object *engine,
		struct nouveau_oclass *bclass, void *data, u32 size,
		struct nouveau_object **pobject)
{
	struct nvc0_graph_oclass *oclass = (void *)bclass;
	struct nouveau_device *device = nv_device(parent);
	struct nvc0_graph_priv *priv;
	bool use_ext_fw, enable;
	int ret, i, j;

	use_ext_fw = nouveau_boolopt(device->cfgopt, "NvGrUseFW",
				     oclass->fecs.ucode == NULL);
	enable = use_ext_fw || oclass->fecs.ucode != NULL;

	ret = nouveau_graph_create(parent, engine, bclass, enable, &priv);
	*pobject = nv_object(priv);
	if (ret)
		return ret;

	nv_subdev(priv)->unit = 0x08001000;
	nv_subdev(priv)->intr = nvc0_graph_intr;

	priv->base.units = nvc0_graph_units;

	if (use_ext_fw) {
		nv_info(priv, "using external firmware\n");
		if (nvc0_graph_ctor_fw(priv, "fuc409c", &priv->fuc409c) ||
		    nvc0_graph_ctor_fw(priv, "fuc409d", &priv->fuc409d) ||
		    nvc0_graph_ctor_fw(priv, "fuc41ac", &priv->fuc41ac) ||
		    nvc0_graph_ctor_fw(priv, "fuc41ad", &priv->fuc41ad))
			return -EINVAL;
		priv->firmware = true;
	}

	ret = nouveau_gpuobj_new(nv_object(priv), NULL, 0x1000, 256, 0,
				&priv->unk4188b4);
	if (ret)
		return ret;

	ret = nouveau_gpuobj_new(nv_object(priv), NULL, 0x1000, 256, 0,
				&priv->unk4188b8);
	if (ret)
		return ret;

	for (i = 0; i < 0x1000; i += 4) {
		nv_wo32(priv->unk4188b4, i, 0x00000010);
		nv_wo32(priv->unk4188b8, i, 0x00000010);
	}

	priv->rop_nr = (nv_rd32(priv, 0x409604) & 0x001f0000) >> 16;
	priv->gpc_nr =  nv_rd32(priv, 0x409604) & 0x0000001f;
	for (i = 0; i < priv->gpc_nr; i++) {
		priv->tpc_nr[i]  = nv_rd32(priv, GPC_UNIT(i, 0x2608));
		priv->tpc_total += priv->tpc_nr[i];
		priv->ppc_nr[i]  = oclass->ppc_nr;
		for (j = 0; j < priv->ppc_nr[i]; j++) {
			u8 mask = nv_rd32(priv, GPC_UNIT(i, 0x0c30 + (j * 4)));
			priv->ppc_tpc_nr[i][j] = hweight8(mask);
		}
	}

	/*XXX: these need figuring out... though it might not even matter */
	switch (nv_device(priv)->chipset) {
	case 0xc0:
		if (priv->tpc_total == 11) { /* 465, 3/4/4/0, 4 */
			priv->magic_not_rop_nr = 0x07;
		} else
		if (priv->tpc_total == 14) { /* 470, 3/3/4/4, 5 */
			priv->magic_not_rop_nr = 0x05;
		} else
		if (priv->tpc_total == 15) { /* 480, 3/4/4/4, 6 */
			priv->magic_not_rop_nr = 0x06;
		}
		break;
	case 0xc3: /* 450, 4/0/0/0, 2 */
		priv->magic_not_rop_nr = 0x03;
		break;
	case 0xc4: /* 460, 3/4/0/0, 4 */
		priv->magic_not_rop_nr = 0x01;
		break;
	case 0xc1: /* 2/0/0/0, 1 */
		priv->magic_not_rop_nr = 0x01;
		break;
	case 0xc8: /* 4/4/3/4, 5 */
		priv->magic_not_rop_nr = 0x06;
		break;
	case 0xce: /* 4/4/0/0, 4 */
		priv->magic_not_rop_nr = 0x03;
		break;
	case 0xcf: /* 4/0/0/0, 3 */
		priv->magic_not_rop_nr = 0x03;
		break;
	case 0xd7:
	case 0xd9: /* 1/0/0/0, 1 */
		priv->magic_not_rop_nr = 0x01;
		break;
	}

	nv_engine(priv)->cclass = *oclass->cclass;
	nv_engine(priv)->sclass =  oclass->sclass;
	return 0;
}

#include "fuc/hubnvc0.fuc.h"

struct nvc0_graph_ucode
nvc0_graph_fecs_ucode = {
	.code.data = nvc0_grhub_code,
	.code.size = sizeof(nvc0_grhub_code),
	.data.data = nvc0_grhub_data,
	.data.size = sizeof(nvc0_grhub_data),
};

#include "fuc/gpcnvc0.fuc.h"

struct nvc0_graph_ucode
nvc0_graph_gpccs_ucode = {
	.code.data = nvc0_grgpc_code,
	.code.size = sizeof(nvc0_grgpc_code),
	.data.data = nvc0_grgpc_data,
	.data.size = sizeof(nvc0_grgpc_data),
};

struct nouveau_oclass *
nvc0_graph_oclass = &(struct nvc0_graph_oclass) {
	.base.handle = NV_ENGINE(GR, 0xc0),
	.base.ofuncs = &(struct nouveau_ofuncs) {
		.ctor = nvc0_graph_ctor,
		.dtor = nvc0_graph_dtor,
		.init = nvc0_graph_init,
		.fini = _nouveau_graph_fini,
	},
	.cclass = &nvc0_grctx_oclass,
	.sclass =  nvc0_graph_sclass,
	.mmio = nvc0_graph_pack_mmio,
	.fecs.ucode = &nvc0_graph_fecs_ucode,
	.gpccs.ucode = &nvc0_graph_gpccs_ucode,
}.base;
