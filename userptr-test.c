#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define __user
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "i915_drm.h"

struct type_name {
	int type;
	char *name;
};
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define type_name_fn(res) \
char * res##_str(int type) {			\
	unsigned int i;					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) { \
		if (res##_names[i].type == type)	\
			return res##_names[i].name;	\
	}						\
	return "(invalid)";				\
}

struct type_name connector_type_names[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
};

type_name_fn(connector_type)

int
main(int argc, char **argv)
{
    int fd;
    uint32_t handle, stride;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo *mode;
    int i;
    unsigned int fb_id;

    struct drm_i915_gem_userptr create;

    fd = open("/dev/dri/card0", O_RDWR);
    drmSetMaster(fd);

    /* Find the first available connector with modes */
 
    resources = drmModeGetResources(fd);
    if (!resources) {
        fprintf(stderr, "drmModeGetResources failed\n");
        return -1;
    }
 
    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector == NULL)
            continue;
 
        if (connector->connection == DRM_MODE_CONNECTED &&
            connector->count_modes > 0)
            break;
 
        drmModeFreeConnector(connector);
    }
 
    if (i == resources->count_connectors) {
        fprintf(stderr, "No currently active connector found.\n");
        return -1;
    }

    printf("Using connector: %s\n", connector_type_str(connector->connector_type));
 
    for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);
 
        if (encoder == NULL)
  	 continue;
 
       if (encoder->encoder_id == connector->encoder_id)
 	 break;
 
       drmModeFreeEncoder(encoder);
    }

    for (i = 0; i < connector->count_modes; i++) {
        mode = &connector->modes[i];
        if (!strcmp(mode->name, "1266x768"))
            break;
    }

    printf("Using mode: %s\n", mode->name);

    create.user_size = ((1366*768*4) + 4095) & ~4095;
    create.user_ptr = (uint64_t)malloc(create.user_size);

    for (i = 0; i < 768; i++) {
        int j;
        for (j = 0; j < 1366; j++) {
	    uint8_t r, g, b;
            int8_t sg, sb;
            double distance, angle;
            uint32_t *buf = (uint32_t *)create.user_ptr;

            distance = sqrt(pow(525 - i, 2) + pow(840 - j,2));
            angle = atan2(525 - i, 840 - j);

            if (distance < 525) {
                r = 256 - ((256 * distance) / 525);
                sg = 256.0 * ((double)angle / 3.14 / 2);
                sb = 256.0 * ((double)angle / 3.14);
                g = abs(sg*2);
                b = abs(sb*2);
                buf[(768 * i) + j] = r << 16 | g << 8 | b;       // ARGB
//                buf[(1680 * i) + j] = b << 16 | b << 8 | r;       // ABGR
            } else {
                if ( (j % 200) < 50 ) 
                    buf[(1366 * i) + j] = 0xFF << 16;
                else if ( (j % 200) < 100)
                    buf[(1366 * i) + j] = 0xFF << 8;
                else if ( (j % 200) < 150)
                    buf[(1366 * i) + j] = 0xFF;
                else
                    buf[(1366 * i) + j] = 0xFFFFFF;
            }
        }
    }

    drmIoctl(fd, DRM_IOCTL_I915_GEM_USERPTR, &create);
    handle = create.handle;

    /* Create a KMS framebuffer handle to set a mode with */
    drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32, 1366*4, handle, &fb_id);

    drmModeSetCrtc(fd, encoder->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, mode);

    drmDropMaster(fd);

    while (1)
        sleep(1);
    return 0;
}
