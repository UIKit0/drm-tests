#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/time.h>

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

#define MAX_VMS 4

int
main(int argc, char **argv)
{
    int fd;
    uint32_t handle[MAX_VMS], stride[MAX_VMS];
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo *mode;
    int i;
    uint32_t fb_id[MAX_VMS], total_vms = 0, current_vm;

    struct drm_i915_gem_vgtfb create;

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
        if (!strcmp(mode->name, "1680x1050"))
            break;
    }

    printf("Using mode: %s\n", mode->name);


    for (i = 0; i < (argc - 1) && i < MAX_VMS; i++) {
	int ret;
        create.vmid = atoi(argv[i+1]);

        printf("Requesting object for VM %d\n", create.vmid);

        if (ret = drmIoctl(fd, DRM_IOCTL_I915_GEM_VGTFB, &create)) {
		printf("drmIoctl failed for domain %d. ret = %d\n", create.vmid, ret);
		continue;
	}
        handle[i] = create.handle;
        stride[i] = create.stride;

        /* Create a KMS framebuffer handle to set a mode with */
        if (ret = drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32, stride[i], handle[i], &fb_id[i])) {
		printf("drmModeAddFB failed for handle %d. ret = %d\n", handle[i], ret);
		continue;
	}
        total_vms++;
    }

    drmDropMaster(fd);

    if (total_vms == 0)
	return -1;

    current_vm = 0;
    do {
        drmSetMaster(fd);
        drmModeSetCrtc(fd, encoder->crtc_id, fb_id[current_vm], 0, 0, &connector->connector_id, 1, mode);
        current_vm++;
        current_vm %= total_vms;
        drmDropMaster(fd);
    } while (getchar() != EOF);


    return 0;
}
