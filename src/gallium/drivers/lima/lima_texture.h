/*
 * Copyright (c) 2018-2019 Lima Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef H_LIMA_TEXTURE
#define H_LIMA_TEXTURE

#define lima_min_tex_desc_size 64

typedef struct __attribute__((__packed__)) {
   /* Word 0 */
   uint32_t format : 6;
   uint32_t flag1: 1;
   uint32_t swap_r_b: 1;
   uint32_t unknown_0_1: 10;
   uint32_t stride: 13;
   uint32_t unknown_0_2: 1;

   /* Word 1-3 */
   uint32_t unknown_1_1: 10;
   uint32_t texture_2d: 1;
   uint32_t unknown_1_2: 13;
   uint32_t miplevels: 4;
   uint32_t min_mipfilter_1: 9; /* 0x0 for linear, 0x1ff for nearest */
   uint32_t unknown_2_1: 3;
   uint32_t has_stride: 1;
   uint32_t min_mipfilter_2: 2; /* 0x3 for linear, 0x0 for nearest */
   uint32_t min_img_filter_nearest: 1;
   uint32_t mag_img_filter_nearest: 1;
   uint32_t wrap_s_clamp_to_edge: 1;
   uint32_t wrap_s_clamp: 1;
   uint32_t wrap_s_mirror_repeat: 1;
   uint32_t wrap_t_clamp_to_edge: 1;
   uint32_t wrap_t_clamp: 1;
   uint32_t wrap_t_mirror_repeat: 1;
   uint32_t unknown_2_2: 3;
   uint32_t width: 13;
   uint32_t height: 13;
   uint32_t unknown_3_1: 1;
   uint32_t unknown_3_2: 15;

   /* Word 4 */
   uint32_t unknown_4;

   /* Word 5 */
   uint32_t unknown_5;

   /* Word 6-15 */
   /* layout is in va[0] bit 13-14 */
   /* VAs start in va[0] at bit 30, each VA is 26 bits (only MSBs are stored), stored
    * linearly in memory */
   union {
      uint32_t va[0];
      struct __attribute__((__packed__)) {
         uint32_t unknown_6_1: 13;
         uint32_t layout: 2;
         uint32_t unknown_6_2: 9;
         uint32_t unknown_6_3: 6;
#define VA_BIT_OFFSET 30
#define VA_BIT_SIZE 26
         uint32_t va_0: VA_BIT_SIZE;
         uint32_t va_0_1: 8;
         uint32_t va_1_x[0];
      } va_s;
   };
} lima_tex_desc;

void lima_texture_desc_set_res(struct lima_context *ctx, lima_tex_desc *desc,
                               struct pipe_resource *prsc,
                               unsigned first_level, unsigned last_level);
void lima_update_textures(struct lima_context *ctx);

#endif
