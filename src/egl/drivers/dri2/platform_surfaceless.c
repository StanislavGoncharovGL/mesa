/*
 * Mesa 3-D graphics library
 *
 * Copyright (c) 2014 The Chromium OS Authors.
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "egl_dri2.h"
#include "egl_dri2_fallbacks.h"
#include "loader.h"

static __DRIimage*
surfaceless_alloc_image(struct dri2_egl_display *dri2_dpy,
                     struct dri2_egl_surface *dri2_surf)
{
   return dri2_dpy->image->createImage(
            dri2_dpy->dri_screen,
            dri2_surf->base.Width,
            dri2_surf->base.Height,
            dri2_surf->visual,
            0,
            NULL);
}

static void
surfaceless_free_images(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   if (dri2_surf->front) {
      dri2_dpy->image->destroyImage(dri2_surf->front);
      dri2_surf->front = NULL;
   }
}

static int
surfaceless_image_get_buffers(__DRIdrawable *driDrawable,
                        unsigned int format,
                        uint32_t *stamp,
                        void *loaderPrivate,
                        uint32_t buffer_mask,
                        struct __DRIimageList *buffers)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   buffers->image_mask = 0;
   buffers->front = NULL;
   buffers->back = NULL;

   /* The EGL 1.5 spec states that pbuffers are single-buffered. Specifically,
    * the spec states that they have a back buffer but no front buffer, in
    * contrast to pixmaps, which have a front buffer but no back buffer.
    *
    * Single-buffered surfaces with no front buffer confuse Mesa; so we deviate
    * from the spec, following the precedent of Mesa's EGL X11 platform. The
    * X11 platform correctly assigns pbuffers to single-buffered configs, but
    * assigns the pbuffer a front buffer instead of a back buffer.
    *
    * Pbuffers in the X11 platform mostly work today, so let's just copy its
    * behavior instead of trying to fix (and hence potentially breaking) the
    * world.
    */

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {

      if (!dri2_surf->front)
         dri2_surf->front =
            surfaceless_alloc_image(dri2_dpy, dri2_surf);

      buffers->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      buffers->front = dri2_surf->front;
   }

   return 1;
}

static _EGLSurface *
dri2_surfaceless_create_surface(_EGLDriver *drv, _EGLDisplay *disp, EGLint type,
                        _EGLConfig *conf, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;

   /* Make sure to calloc so all pointers
    * are originally NULL.
    */
   dri2_surf = calloc(1, sizeof *dri2_surf);

   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "eglCreatePbufferSurface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, type, conf, attrib_list,
                          false, NULL))
      goto cleanup_surface;

   config = dri2_get_dri_config(dri2_conf, type,
                                dri2_surf->base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surface;
   }

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surface;

   if (conf->RedSize == 5)
      dri2_surf->visual = __DRI_IMAGE_FORMAT_RGB565;
   else if (conf->AlphaSize == 0)
      dri2_surf->visual = __DRI_IMAGE_FORMAT_XRGB8888;
   else
      dri2_surf->visual = __DRI_IMAGE_FORMAT_ARGB8888;

   return &dri2_surf->base;

   cleanup_surface:
      free(dri2_surf);
      return NULL;
}

static EGLBoolean
surfaceless_destroy_surface(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   surfaceless_free_images(dri2_surf);

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

   dri2_fini_surface(surf);
   free(dri2_surf);
   return EGL_TRUE;
}

static _EGLSurface *
dri2_surfaceless_create_pbuffer_surface(_EGLDriver *drv, _EGLDisplay *disp,
                                _EGLConfig *conf, const EGLint *attrib_list)
{
   return dri2_surfaceless_create_surface(drv, disp, EGL_PBUFFER_BIT, conf,
                                  attrib_list);
}

static EGLBoolean
surfaceless_add_configs_for_visuals(_EGLDriver *drv, _EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   static const struct {
      const char *format_name;
      int rgba_shifts[4];
      unsigned int rgba_sizes[4];
   } visuals[] = {
      { "A2RGB10",  { 20, 10, 0, 30 }, { 10, 10, 10, 2 } },
      { "X2RGB10",  { 20, 10, 0, -1 }, { 10, 10, 10, 0 } },
      { "ARGB8888", { 16, 8, 0, 24 }, { 8, 8, 8, 8 } },
      { "RGB888",   { 16, 8, 0, -1 }, { 8, 8, 8, 0 } },
      { "RGB565",   { 11, 5, 0, -1 }, { 5, 6, 5, 0 } },
   };
   unsigned int format_count[ARRAY_SIZE(visuals)] = { 0 };
   unsigned int config_count = 0;

   for (unsigned i = 0; dri2_dpy->driver_configs[i] != NULL; i++) {
      for (unsigned j = 0; j < ARRAY_SIZE(visuals); j++) {
         struct dri2_egl_config *dri2_conf;

         dri2_conf = dri2_add_config(disp, dri2_dpy->driver_configs[i],
               config_count + 1, EGL_PBUFFER_BIT, NULL,
               visuals[j].rgba_shifts, visuals[j].rgba_sizes);

         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == config_count + 1)
               config_count++;
            format_count[j]++;
         }
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_DEBUG, "No DRI config supports native format %s",
               visuals[i].format_name);
      }
   }

   return (config_count != 0);
}

static const struct dri2_egl_display_vtbl dri2_surfaceless_display_vtbl = {
   .create_pixmap_surface = dri2_fallback_create_pixmap_surface,
   .create_pbuffer_surface = dri2_surfaceless_create_pbuffer_surface,
   .destroy_surface = surfaceless_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri2_fallback_copy_buffers,
   .query_buffer_age = dri2_fallback_query_buffer_age,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri2_fallback_get_sync_values,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static void
surfaceless_flush_front_buffer(__DRIdrawable *driDrawable, void *loaderPrivate)
{
}

static const __DRIimageLoaderExtension image_loader_extension = {
   .base             = { __DRI_IMAGE_LOADER, 1 },
   .getBuffers       = surfaceless_image_get_buffers,
   .flushFrontBuffer = surfaceless_flush_front_buffer,
};

static const __DRIextension *image_loader_extensions[] = {
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static const __DRIextension *swrast_loader_extensions[] = {
   &swrast_pbuffer_loader_extension.base,
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static bool
surfaceless_probe_device(_EGLDisplay *disp, bool swrast)
{
#define MAX_DRM_DEVICES 64
   const unsigned node_type = swrast ? DRM_NODE_PRIMARY : DRM_NODE_RENDER;
   struct dri2_egl_display *dri2_dpy = disp->DriverData;
   drmDevicePtr device, devices[MAX_DRM_DEVICES] = { NULL };
   int i, num_devices;

   num_devices = drmGetDevices2(0, devices, ARRAY_SIZE(devices));
   if (num_devices < 0)
      return false;

   for (i = 0; i < num_devices; ++i) {
      device = devices[i];

      if (!(device->available_nodes & (1 << node_type)))
         continue;

      dri2_dpy->fd = loader_open_device(device->nodes[node_type]);
      if (dri2_dpy->fd < 0)
         continue;

      disp->Device = _eglAddDevice(dri2_dpy->fd, swrast);
      if (!disp->Device) {
         close(dri2_dpy->fd);
         dri2_dpy->fd = -1;
         continue;
      }

      char *driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
      if (swrast) {
         /* Use kms swrast only with vgem / virtio_gpu.
          * virtio-gpu fallbacks to software rendering when 3D features
          * are unavailable since 6c5ab, and kms_swrast is more
          * feature complete than swrast.
          */
         if (driver_name &&
             (strcmp(driver_name, "vgem") == 0 ||
              strcmp(driver_name, "virtio_gpu") == 0))
            dri2_dpy->driver_name = strdup("kms_swrast");
         free(driver_name);
      } else {
         /* Use the given hardware driver */
         dri2_dpy->driver_name = driver_name;
      }

      if (dri2_dpy->driver_name && dri2_load_driver_dri3(disp))
         break;

      free(dri2_dpy->driver_name);
      dri2_dpy->driver_name = NULL;
      close(dri2_dpy->fd);
      dri2_dpy->fd = -1;
   }
   drmFreeDevices(devices, num_devices);

   if (i == num_devices)
      return false;

   if (swrast)
      dri2_dpy->loader_extensions = swrast_loader_extensions;
   else
      dri2_dpy->loader_extensions = image_loader_extensions;

   return true;
}

static bool
surfaceless_probe_device_sw(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = disp->DriverData;

   dri2_dpy->fd = -1;
   disp->Device = _eglAddDevice(dri2_dpy->fd, true);
   assert(disp->Device);

   dri2_dpy->driver_name = strdup("swrast");
   if (!dri2_dpy->driver_name)
      return false;

   if (!dri2_load_driver_swrast(disp)) {
      free(dri2_dpy->driver_name);
      dri2_dpy->driver_name = NULL;
      return false;
   }

   dri2_dpy->loader_extensions = swrast_loader_extensions;
   return true;
}

EGLBoolean
dri2_initialize_surfaceless(_EGLDriver *drv, _EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy;
   const char* err;
   bool driver_loaded = false;

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   disp->DriverData = (void *) dri2_dpy;

   if (!disp->Options.ForceSoftware) {
      driver_loaded = surfaceless_probe_device(disp, false);
      if (!driver_loaded)
         _eglLog(_EGL_WARNING,
                 "No hardware driver found, falling back to software rendering");
   }

   if (!driver_loaded)
      driver_loaded = surfaceless_probe_device(disp, true);

   if (!driver_loaded) {
      _eglLog(_EGL_DEBUG, "Falling back to surfaceless swrast without DRM.");
      if (!surfaceless_probe_device_sw(disp)) {
         err = "DRI2: failed to load driver";
         goto cleanup;
      }
   }

   if (!dri2_create_screen(disp)) {
      err = "DRI2: failed to create screen";
      goto cleanup;
   }

   if (!dri2_setup_extensions(disp)) {
      err = "DRI2: failed to find required DRI extensions";
      goto cleanup;
   }

   dri2_setup_screen(disp);

   if (!surfaceless_add_configs_for_visuals(drv, disp)) {
      err = "DRI2: failed to add configs";
      goto cleanup;
   }

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_surfaceless_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, err);
}
