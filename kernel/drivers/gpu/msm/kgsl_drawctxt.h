#ifndef __GSL_DRAWCTXT_H
#define __GSL_DRAWCTXT_H

#define CTXT_FLAGS_NOT_IN_USE		0x00000000
#define CTXT_FLAGS_IN_USE		0x00000001
#define CTXT_FLAGS_STATE_SHADOW		0x00000010
#define CTXT_FLAGS_GMEM_SHADOW		0x00000100
#define CTXT_FLAGS_GMEM_SAVE		0x00000200
#define CTXT_FLAGS_GMEM_RESTORE		0x00000400
#define CTXT_FLAGS_SHADER_SAVE		0x00002000
#define CTXT_FLAGS_SHADER_RESTORE	0x00004000

#include <linux/msm_kgsl.h>
#include "kgsl_sharedmem.h"
#include "yamato_reg.h"

#define KGSL_MAX_GMEM_SHADOW_BUFFERS	2

struct kgsl_device;
struct kgsl_yamato_device;
struct kgsl_device_private;
struct kgsl_context;

struct gmem_shadow_t {
	struct kgsl_memdesc gmemshadow;	/* Shadow buffer address */
	enum COLORFORMATX format;
	unsigned int size;	/* Size of surface used to store GMEM */
	unsigned int width;	/* Width of surface used to store GMEM */
	unsigned int height;	/* Height of surface used to store GMEM */
	unsigned int pitch;	/* Pitch of surface used to store GMEM */
	int offset;
	unsigned int offset_x;
	unsigned int offset_y;
	unsigned int gmem_offset_x;
	unsigned int gmem_offset_y;
	unsigned int gmem_pitch;	/* Pitch value used for GMEM */
	unsigned int *gmem_save_commands;
	unsigned int *gmem_restore_commands;
	unsigned int gmem_save[3];
	unsigned int gmem_restore[3];
	struct kgsl_memdesc quad_vertices;
	struct kgsl_memdesc quad_texcoords;
};

struct kgsl_yamato_context {
	uint32_t         flags;
	struct kgsl_pagetable *pagetable;
	struct kgsl_memdesc       gpustate;
	unsigned int        reg_save[3];
	unsigned int        reg_restore[3];
	unsigned int        shader_save[3];
	unsigned int        shader_fixup[3];
	unsigned int        shader_restore[3];
	unsigned int		chicken_restore[3];
	unsigned int 	    bin_base_offset;
	struct gmem_shadow_t context_gmem_shadow;
	struct gmem_shadow_t user_gmem_shadow[KGSL_MAX_GMEM_SHADOW_BUFFERS];
};


int kgsl_drawctxt_create(struct kgsl_device_private *dev_priv,
			 uint32_t flags,
			 struct kgsl_context *context);

int kgsl_drawctxt_destroy(struct kgsl_device *device,
			  struct kgsl_context *context);

int kgsl_drawctxt_init(struct kgsl_device *device);

int kgsl_drawctxt_close(struct kgsl_device *device);

void kgsl_drawctxt_switch(struct kgsl_yamato_device *yamato_device,
				struct kgsl_yamato_context *drawctxt,
				unsigned int flags);
int kgsl_drawctxt_bind_gmem_shadow(struct kgsl_yamato_device *yamato_device,
			struct kgsl_context *context,
			const struct kgsl_gmem_desc *gmem_desc,
			unsigned int shadow_x,
			unsigned int shadow_y,
			const struct kgsl_buffer_desc
			*shadow_buffer, unsigned int buffer_id);

int kgsl_drawctxt_set_bin_base_offset(struct kgsl_device *device,
				      struct kgsl_context *context,
					unsigned int offset);

#endif
