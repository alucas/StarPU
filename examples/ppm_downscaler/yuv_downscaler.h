/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2010 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#define WIDTH	1920
#define HEIGHT	1080

#define FACTOR	2

#define NEW_WIDTH	(WIDTH/FACTOR)
#define NEW_HEIGHT	(HEIGHT/FACTOR)

#define BLOCK_HEIGHT    20

#include <stdint.h>

struct yuv_frame {
	uint8_t y[WIDTH*HEIGHT];
	uint8_t u[(WIDTH*HEIGHT)/4];
	uint8_t v[(WIDTH*HEIGHT)/4];
};

struct yuv_new_frame {
	uint8_t y[NEW_WIDTH*NEW_HEIGHT];
	uint8_t u[(NEW_WIDTH*NEW_HEIGHT)/4];
	uint8_t v[(NEW_WIDTH*NEW_HEIGHT)/4];
};
