all: xengt-test userptr-test userptr-fb-test foreign-test

xengt-dri3-test: xengt-dri3-test.c
	gcc -g -I /usr/include/drm -o $@ $< -lm `pkg-config --cflags --libs xcb xcb-dri3 xcb-aux libdrm_intel`

userptr-dri3-test: userptr-dri3-test.c
	gcc -g -I /usr/include/drm -o $@ $< -lm `pkg-config --cflags --libs xcb xcb-dri3 xcb-aux libdrm_intel`

%: %.c
	gcc -g -I /usr/include/drm -I /home/jbaboval/sandbox/xengt-tree/xen/dist/install/usr/include/ -o $@ $< -L /home/jbaboval/sandbox/xengt-tree/xen/dist/install/usr/lib/x86_64-linux-gnu -lxenctrl -ldrm -lm
