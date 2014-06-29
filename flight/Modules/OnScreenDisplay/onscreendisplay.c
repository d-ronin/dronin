/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup OSDgenModule osdgen Module
 * @brief Process OSD information
 * @{
 *
 * @file       osdgen.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      OSD gen module, handles OSD draw. Parts from CL-OSD and SUPEROSD projects
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

// ****************

// #define DEBUG_TIMING
// #define DEBUG_ALARMS
// #define DEBUG_TELEMETRY
// #define DEBUG_BLACK_WHITE
// #define DEBUG_STUFF
#define SIMULATE_DATA
#define TEMP_GPS_STATUS_WORKAROUND

#include <openpilot.h>

#include "onscreendisplay.h"
#include "onscreendisplaysettings.h"
#include "onscreendisplaypagesettings.h"
#include "modulesettings.h"
#include "pios_video.h"

#include "physical_constants.h"
#include "sin_lookup.h"

#include "accessorydesired.h"
#include "attitudeactual.h"
#include "flightstatus.h"
#include "flightstatus.h"
#include "flightbatterystate.h"
#include "gpsposition.h"
#include "positionactual.h"
#include "gpstime.h"
#include "gpssatellites.h"
#include "homelocation.h"
#include "manualcontrolcommand.h"
#include "systemalarms.h"
#include "systemstats.h"
#include "taskinfo.h"
#include "velocityactual.h"

#include "fonts.h"
#include "font12x18.h"
#include "font8x10.h"
#include "WMMInternal.h"
#include "logos.h"

extern uint8_t PIOS_Board_Revision(void);

extern uint8_t *draw_buffer_level;
extern uint8_t *draw_buffer_mask;
extern uint8_t *disp_buffer_level;
extern uint8_t *disp_buffer_mask;

// ****************
// Private functions
static void onScreenDisplayTask(void *parameters);

// ****************
// Private constants
#define LONG_TIME        0xffff
#define STACK_SIZE_BYTES 4096
#define TASK_PRIORITY    (tskIDLE_PRIORITY + 1)
#define UPDATE_PERIOD    100
#define BLINK_INTERVAL_FRAMES 6

// Unit conversion constants
#define MS_TO_KMH 3.6f
#define MS_TO_MPH 2.23694f
#define M_TO_FEET 3.28084f



// ****************
// Private variables
static uint16_t frame_counter = 0;
static bool module_enabled = false;
static xTaskHandle onScreenDisplayTaskHandle;
xSemaphoreHandle onScreenDisplaySemaphore = NULL;
uint8_t module_state[MODULESETTINGS_ADMINSTATE_NUMELEM];
float convert_speed;
float convert_distance;
static volatile bool osd_settings_updated = true;

#ifdef DEBUG_TIMING
static portTickType in_ticks  = 0;
static portTickType out_ticks = 0;
static uint16_t in_time  = 0;
static uint16_t out_time = 0;
#endif


void clearGraphics()
{
	memset((uint8_t *)draw_buffer_mask, 0, BUFFER_HEIGHT * BUFFER_WIDTH);
	memset((uint8_t *)draw_buffer_level, 0, BUFFER_HEIGHT * BUFFER_WIDTH);
}

void draw_image(uint16_t x, uint16_t y, const struct Image * image)
{
	x /= 8;  // column position in bytes
	uint8_t byte_width = image->width / 8;

	for (uint16_t xp = 0; xp < image->width / 8; xp++){
		for (uint16_t yp = 0; yp < image->height; yp++){
			draw_buffer_level[(y + yp) * BUFFER_WIDTH + xp + x] = image->level[yp * byte_width + xp];
			draw_buffer_mask[(y + yp) * BUFFER_WIDTH + xp + x] = image->mask[yp * byte_width + xp];
		}
	}
}

/// Draws four points relative to the given center point.
///
/// \li centerX + X, centerY + Y
/// \li centerX + X, centerY - Y
/// \li centerX - X, centerY + Y
/// \li centerX - X, centerY - Y
///
/// \param centerX the x coordinate of the center point
/// \param centerY the y coordinate of the center point
/// \param deltaX the difference between the centerX coordinate and each pixel drawn
/// \param deltaY the difference between the centerY coordinate and each pixel drawn
/// \param color the color to draw the pixels with.
void plotFourQuadrants(int32_t centerX, int32_t centerY, int32_t deltaX, int32_t deltaY)
{
	write_pixel_lm(centerX + deltaX, centerY + deltaY, 1, 1); // Ist      Quadrant
	write_pixel_lm(centerX - deltaX, centerY + deltaY, 1, 1); // IInd     Quadrant
	write_pixel_lm(centerX - deltaX, centerY - deltaY, 1, 1); // IIIrd    Quadrant
	write_pixel_lm(centerX + deltaX, centerY - deltaY, 1, 1); // IVth     Quadrant
}

/// Implements the midpoint ellipse drawing algorithm which is a bresenham
/// style DDF.
///
/// \param centerX the x coordinate of the center of the ellipse
/// \param centerY the y coordinate of the center of the ellipse
/// \param horizontalRadius the horizontal radius of the ellipse
/// \param verticalRadius the vertical radius of the ellipse
/// \param color the color of the ellipse border
void ellipse(int centerX, int centerY, int horizontalRadius, int verticalRadius)
{
	int64_t doubleHorizontalRadius = horizontalRadius * horizontalRadius;
	int64_t doubleVerticalRadius   = verticalRadius * verticalRadius;

	int64_t error = doubleVerticalRadius - doubleHorizontalRadius * verticalRadius + (doubleVerticalRadius >> 2);

	int x = 0;
	int y = verticalRadius;
	int deltaX = 0;
	int deltaY = (doubleHorizontalRadius << 1) * y;

	plotFourQuadrants(centerX, centerY, x, y);

	while (deltaY >= deltaX) {
		x++;
		deltaX += (doubleVerticalRadius << 1);

		error  += deltaX + doubleVerticalRadius;

		if (error >= 0) {
			y--;
			deltaY -= (doubleHorizontalRadius << 1);

			error  -= deltaY;
		}
		plotFourQuadrants(centerX, centerY, x, y);
	}

	error = (int64_t)(doubleVerticalRadius * (x + 1 / 2.0f) * (x + 1 / 2.0f) + doubleHorizontalRadius * (y - 1) * (y - 1) - doubleHorizontalRadius * doubleVerticalRadius);

	while (y >= 0) {
		error  += doubleHorizontalRadius;
		y--;
		deltaY -= (doubleHorizontalRadius << 1);
		error  -= deltaY;

		if (error <= 0) {
			x++;
			deltaX += (doubleVerticalRadius << 1);
			error  += deltaX;
		}

		plotFourQuadrants(centerX, centerY, x, y);
	}
}

void drawArrow(uint16_t x, uint16_t y, uint16_t angle, uint16_t size_quarter)
{
	float sin_angle = sin_lookup_deg(angle);
	float cos_angle = cos_lookup_deg(angle);
	int16_t peak_x  = (int16_t)(sin_angle * size_quarter * 2);
	int16_t peak_y  = (int16_t)(cos_angle * size_quarter * 2);
	int16_t d_end_x = (int16_t)(cos_angle * size_quarter);
	int16_t d_end_y = (int16_t)(sin_angle * size_quarter);

	write_line_lm(x + peak_x, y - peak_y, x - peak_x - d_end_x, y + peak_y - d_end_y, 1, 1);
	write_line_lm(x + peak_x, y - peak_y, x - peak_x + d_end_x, y + peak_y + d_end_y, 1, 1);
	write_line_lm(x, y, x - peak_x - d_end_x, y + peak_y - d_end_y, 1, 1);
	write_line_lm(x, y, x - peak_x + d_end_x, y + peak_y + d_end_y, 1, 1);
}

void drawBox(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	write_line_lm(x1, y1, x2, y1, 1, 1); // top
	write_line_lm(x1, y1, x1, y2, 1, 1); // left
	write_line_lm(x2, y1, x2, y2, 1, 1); // right
	write_line_lm(x1, y2, x2, y2, 1, 1); // bottom
}

/**
 * write_pixel: Write a pixel at an x,y position to a given surface.
 *
 * @param       buff    pointer to buffer to write in
 * @param       x               x coordinate
 * @param       y               y coordinate
 * @param       mode    0 = clear bit, 1 = set bit, 2 = toggle bit
 */
void write_pixel(uint8_t *buff, int x, int y, int mode)
{
	CHECK_COORDS(x, y);
	// Determine the bit in the word to be set and the word
	// index to set it in.
	int bitnum    = CALC_BIT_IN_WORD(x);
	int wordnum   = CALC_BUFF_ADDR(x, y);
	// Apply a mask.
	uint16_t mask = 1 << (7 - bitnum);
	WRITE_WORD_MODE(buff, wordnum, mask, mode);
}

/**
 * write_pixel_lm: write the pixel on both surfaces (level and mask.)
 * Uses current draw buffer.
 *
 * @param       x               x coordinate
 * @param       y               y coordinate
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 * @param       lmode   0 = black, 1 = white, 2 = toggle
 */
void write_pixel_lm(int x, int y, int mmode, int lmode)
{
	CHECK_COORDS(x, y);
	// Determine the bit in the word to be set and the word
	// index to set it in.
	int bitnum    = CALC_BIT_IN_WORD(x);
	int wordnum   = CALC_BUFF_ADDR(x, y);
	// Apply the masks.
	uint16_t mask = 1 << (7 - bitnum);
	WRITE_WORD_MODE(draw_buffer_mask, wordnum, mask, mmode);
	WRITE_WORD_MODE(draw_buffer_level, wordnum, mask, lmode);
}

/**
 * write_hline: optimised horizontal line writing algorithm
 *
 * @param       buff    pointer to buffer to write in
 * @param       x0      x0 coordinate
 * @param       x1      x1 coordinate
 * @param       y       y coordinate
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_hline(uint8_t *buff, int x0, int x1, int y, int mode)
{
	CHECK_COORD_Y(y);
	CLIP_COORD_X(x0);
	CLIP_COORD_X(x1);
	if (x0 > x1) {
		SWAP(x0, x1);
	}
	if (x0 == x1) {
		return;
	}
	/* This is an optimised algorithm for writing horizontal lines.
	 * We begin by finding the addresses of the x0 and x1 points. */
	int addr0     = CALC_BUFF_ADDR(x0, y);
	int addr1     = CALC_BUFF_ADDR(x1, y);
	int addr0_bit = CALC_BIT_IN_WORD(x0);
	int addr1_bit = CALC_BIT_IN_WORD(x1);
	int mask, mask_l, mask_r, i;
	/* If the addresses are equal, we only need to write one word
	 * which is an island. */
	if (addr0 == addr1) {
		mask = COMPUTE_HLINE_ISLAND_MASK(addr0_bit, addr1_bit);
		WRITE_WORD_MODE(buff, addr0, mask, mode);
	} else {
		/* Otherwise we need to write the edges and then the middle. */
		mask_l = COMPUTE_HLINE_EDGE_L_MASK(addr0_bit);
		mask_r = COMPUTE_HLINE_EDGE_R_MASK(addr1_bit);
		WRITE_WORD_MODE(buff, addr0, mask_l, mode);
		WRITE_WORD_MODE(buff, addr1, mask_r, mode);
		// Now write 0xffff words from start+1 to end-1.
		for (i = addr0 + 1; i <= addr1 - 1; i++) {
			uint8_t m = 0xff;
			WRITE_WORD_MODE(buff, i, m, mode);
		}
	}
}

/**
 * write_hline_lm: write both level and mask buffers.
 *
 * @param       x0              x0 coordinate
 * @param       x1              x1 coordinate
 * @param       y               y coordinate
 * @param       lmode   0 = clear, 1 = set, 2 = toggle
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_hline_lm(int x0, int x1, int y, int lmode, int mmode)
{
	// TODO: an optimisation would compute the masks and apply to
	// both buffers simultaneously.
	write_hline(draw_buffer_level, x0, x1, y, lmode);
	write_hline(draw_buffer_mask, x0, x1, y, mmode);
}

/**
 * write_hline_outlined: outlined horizontal line with varying endcaps
 * Always uses draw buffer.
 *
 * @param       x0                      x0 coordinate
 * @param       x1                      x1 coordinate
 * @param       y                       y coordinate
 * @param       endcap0         0 = none, 1 = single pixel, 2 = full cap
 * @param       endcap1         0 = none, 1 = single pixel, 2 = full cap
 * @param       mode            0 = black outline, white body, 1 = white outline, black body
 * @param       mmode           0 = clear, 1 = set, 2 = toggle
 */
void write_hline_outlined(int x0, int x1, int y, int endcap0, int endcap1, int mode, int mmode)
{
	int stroke, fill;

	SETUP_STROKE_FILL(stroke, fill, mode);
	if (x0 > x1) {
		SWAP(x0, x1);
	}
	// Draw the main body of the line.
	write_hline_lm(x0 + 1, x1 - 1, y - 1, stroke, mmode);
	write_hline_lm(x0 + 1, x1 - 1, y + 1, stroke, mmode);
	write_hline_lm(x0 + 1, x1 - 1, y, fill, mmode);
	// Draw the endcaps, if any.
	DRAW_ENDCAP_HLINE(endcap0, x0, y, stroke, fill, mmode);
	DRAW_ENDCAP_HLINE(endcap1, x1, y, stroke, fill, mmode);
}

/**
 * write_vline: optimised vertical line writing algorithm
 *
 * @param       buff    pointer to buffer to write in
 * @param       x       x coordinate
 * @param       y0      y0 coordinate
 * @param       y1      y1 coordinate
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_vline(uint8_t *buff, int x, int y0, int y1, int mode)
{
	CHECK_COORD_X(x);
	CLIP_COORD_Y(y0);
	CLIP_COORD_Y(y1);
	if (y0 > y1) {
		SWAP(y0, y1);
	}
	if (y0 == y1) {
		return;
	}
	/* This is an optimised algorithm for writing vertical lines.
	 * We begin by finding the addresses of the x,y0 and x,y1 points. */
	int addr0  = CALC_BUFF_ADDR(x, y0);
	int addr1  = CALC_BUFF_ADDR(x, y1);
	/* Then we calculate the pixel data to be written. */
	int bitnum = CALC_BIT_IN_WORD(x);
	uint16_t mask = 1 << (7 - bitnum);
	/* Run from addr0 to addr1 placing pixels. Increment by the number
	 * of words n each graphics line. */
	for (int a = addr0; a <= addr1; a += BUFFER_WIDTH) {
		WRITE_WORD_MODE(buff, a, mask, mode);
	}
}

/**
 * write_vline_lm: write both level and mask buffers.
 *
 * @param       x               x coordinate
 * @param       y0              y0 coordinate
 * @param       y1              y1 coordinate
 * @param       lmode   0 = clear, 1 = set, 2 = toggle
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_vline_lm(int x, int y0, int y1, int lmode, int mmode)
{
	// TODO: an optimisation would compute the masks and apply to
	// both buffers simultaneously.
	write_vline(draw_buffer_level, x, y0, y1, lmode);
	write_vline(draw_buffer_mask, x, y0, y1, mmode);
}

/**
 * write_vline_outlined: outlined vertical line with varying endcaps
 * Always uses draw buffer.
 *
 * @param       x                       x coordinate
 * @param       y0                      y0 coordinate
 * @param       y1                      y1 coordinate
 * @param       endcap0         0 = none, 1 = single pixel, 2 = full cap
 * @param       endcap1         0 = none, 1 = single pixel, 2 = full cap
 * @param       mode            0 = black outline, white body, 1 = white outline, black body
 * @param       mmode           0 = clear, 1 = set, 2 = toggle
 */
void write_vline_outlined(int x, int y0, int y1, int endcap0, int endcap1, int mode, int mmode)
{
	int stroke, fill;

	if (y0 > y1) {
		SWAP(y0, y1);
	}
	SETUP_STROKE_FILL(stroke, fill, mode);
	// Draw the main body of the line.
	write_vline_lm(x - 1, y0 + 1, y1 - 1, stroke, mmode);
	write_vline_lm(x + 1, y0 + 1, y1 - 1, stroke, mmode);
	write_vline_lm(x, y0 + 1, y1 - 1, fill, mmode);
	// Draw the endcaps, if any.
	DRAW_ENDCAP_VLINE(endcap0, x, y0, stroke, fill, mmode);
	DRAW_ENDCAP_VLINE(endcap1, x, y1, stroke, fill, mmode);
}

/**
 * write_filled_rectangle: draw a filled rectangle.
 *
 * Uses an optimised algorithm which is similar to the horizontal
 * line writing algorithm, but optimised for writing the lines
 * multiple times without recalculating lots of stuff.
 *
 * @param       buff    pointer to buffer to write in
 * @param       x               x coordinate (left)
 * @param       y               y coordinate (top)
 * @param       width   rectangle width
 * @param       height  rectangle height
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_filled_rectangle(uint8_t *buff, int x, int y, int width, int height, int mode)
{
	int yy, addr0_old, addr1_old;

	CHECK_COORDS(x, y);
	CHECK_COORDS(x + width, y + height);
	if (width <= 0 || height <= 0) {
		return;
	}
	// Calculate as if the rectangle was only a horizontal line. We then
	// step these addresses through each row until we iterate `height` times.
	int addr0     = CALC_BUFF_ADDR(x, y);
	int addr1     = CALC_BUFF_ADDR(x + width, y);
	int addr0_bit = CALC_BIT_IN_WORD(x);
	int addr1_bit = CALC_BIT_IN_WORD(x + width);
	int mask, mask_l, mask_r, i;
	// If the addresses are equal, we need to write one word vertically.
	if (addr0 == addr1) {
		mask = COMPUTE_HLINE_ISLAND_MASK(addr0_bit, addr1_bit);
		while (height--) {
			WRITE_WORD_MODE(buff, addr0, mask, mode);
			addr0 += BUFFER_WIDTH;
		}
	} else {
		// Otherwise we need to write the edges and then the middle repeatedly.
		mask_l    = COMPUTE_HLINE_EDGE_L_MASK(addr0_bit);
		mask_r    = COMPUTE_HLINE_EDGE_R_MASK(addr1_bit);
		// Write edges first.
		yy        = 0;
		addr0_old = addr0;
		addr1_old = addr1;
		while (yy < height) {
			WRITE_WORD_MODE(buff, addr0, mask_l, mode);
			WRITE_WORD_MODE(buff, addr1, mask_r, mode);
			addr0 += BUFFER_WIDTH;
			addr1 += BUFFER_WIDTH;
			yy++;
		}
		// Now write 0xffff words from start+1 to end-1 for each row.
		yy    = 0;
		addr0 = addr0_old;
		addr1 = addr1_old;
		while (yy < height) {
			for (i = addr0 + 1; i <= addr1 - 1; i++) {
				uint8_t m = 0xff;
				WRITE_WORD_MODE(buff, i, m, mode);
			}
			addr0 += BUFFER_WIDTH;
			addr1 += BUFFER_WIDTH;
			yy++;
		}
	}
}

/**
 * write_filled_rectangle_lm: draw a filled rectangle on both draw buffers.
 *
 * @param       x               x coordinate (left)
 * @param       y               y coordinate (top)
 * @param       width   rectangle width
 * @param       height  rectangle height
 * @param       lmode   0 = clear, 1 = set, 2 = toggle
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_filled_rectangle_lm(int x, int y, int width, int height, int lmode, int mmode)
{
	write_filled_rectangle(draw_buffer_mask, x, y, width, height, mmode);
	write_filled_rectangle(draw_buffer_level, x, y, width, height, lmode);
}

/**
 * write_rectangle_outlined: draw an outline of a rectangle. Essentially
 * a convenience wrapper for draw_hline_outlined and draw_vline_outlined.
 *
 * @param       x               x coordinate (left)
 * @param       y               y coordinate (top)
 * @param       width   rectangle width
 * @param       height  rectangle height
 * @param       mode    0 = black outline, white body, 1 = white outline, black body
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_rectangle_outlined(int x, int y, int width, int height, int mode, int mmode)
{
	write_hline_outlined(x, x + width, y, ENDCAP_ROUND, ENDCAP_ROUND, mode, mmode);
	write_hline_outlined(x, x + width, y + height, ENDCAP_ROUND, ENDCAP_ROUND, mode, mmode);
	write_vline_outlined(x, y, y + height, ENDCAP_ROUND, ENDCAP_ROUND, mode, mmode);
	write_vline_outlined(x + width, y, y + height, ENDCAP_ROUND, ENDCAP_ROUND, mode, mmode);
}

/**
 * write_circle: draw the outline of a circle on a given buffer,
 * with an optional dash pattern for the line instead of a normal line.
 *
 * @param       buff    pointer to buffer to write in
 * @param       cx              origin x coordinate
 * @param       cy              origin y coordinate
 * @param       r               radius
 * @param       dashp   dash period (pixels) - zero for no dash
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_circle(uint8_t *buff, int cx, int cy, int r, int dashp, int mode)
{
	CHECK_COORDS(cx, cy);
	int error = -r, x = r, y = 0;
	while (x >= y) {
		if (dashp == 0 || (y % dashp) < (dashp / 2)) {
			CIRCLE_PLOT_8(buff, cx, cy, x, y, mode);
		}
		error += (y * 2) + 1;
		y++;
		if (error >= 0) {
			--x;
			error -= x * 2;
		}
	}
}

/**
 * write_circle_outlined: draw an outlined circle on the draw buffer.
 *
 * @param       cx              origin x coordinate
 * @param       cy              origin y coordinate
 * @param       r               radius
 * @param       dashp   dash period (pixels) - zero for no dash
 * @param       bmode   0 = 4-neighbour border, 1 = 8-neighbour border
 * @param       mode    0 = black outline, white body, 1 = white outline, black body
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_circle_outlined(int cx, int cy, int r, int dashp, int bmode, int mode, int mmode)
{
	int stroke, fill;

	CHECK_COORDS(cx, cy);
	SETUP_STROKE_FILL(stroke, fill, mode);
	// This is a two step procedure. First, we draw the outline of the
	// circle, then we draw the inner part.
	int error = -r, x = r, y = 0;
	while (x >= y) {
		if (dashp == 0 || (y % dashp) < (dashp / 2)) {
			CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x + 1, y, mmode);
			CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x + 1, y, stroke);
			CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x, y + 1, mmode);
			CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x, y + 1, stroke);
			CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x - 1, y, mmode);
			CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x - 1, y, stroke);
			CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x, y - 1, mmode);
			CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x, y - 1, stroke);
			if (bmode == 1) {
				CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x + 1, y + 1, mmode);
				CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x + 1, y + 1, stroke);
				CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x - 1, y - 1, mmode);
				CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x - 1, y - 1, stroke);
			}
		}
		error += (y * 2) + 1;
		y++;
		if (error >= 0) {
			--x;
			error -= x * 2;
		}
	}
	error = -r;
	x     = r;
	y     = 0;
	while (x >= y) {
		if (dashp == 0 || (y % dashp) < (dashp / 2)) {
			CIRCLE_PLOT_8(draw_buffer_mask, cx, cy, x, y, mmode);
			CIRCLE_PLOT_8(draw_buffer_level, cx, cy, x, y, fill);
		}
		error += (y * 2) + 1;
		y++;
		if (error >= 0) {
			--x;
			error -= x * 2;
		}
	}
}

/**
 * write_circle_filled: fill a circle on a given buffer.
 *
 * @param       buff    pointer to buffer to write in
 * @param       cx              origin x coordinate
 * @param       cy              origin y coordinate
 * @param       r               radius
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_circle_filled(uint8_t *buff, int cx, int cy, int r, int mode)
{
	CHECK_COORDS(cx, cy);
	int error = -r, x = r, y = 0, xch = 0;
	// It turns out that filled circles can take advantage of the midpoint
	// circle algorithm. We simply draw very fast horizontal lines across each
	// pair of X,Y coordinates. In some cases, this can even be faster than
	// drawing an outlined circle!
	//
	// Due to multiple writes to each set of pixels, we have a special exception
	// for when using the toggling draw mode.
	while (x >= y) {
		if (y != 0) {
			write_hline(buff, cx - x, cx + x, cy + y, mode);
			write_hline(buff, cx - x, cx + x, cy - y, mode);
			if (mode != 2 || (mode == 2 && xch && (cx - x) != (cx - y))) {
				write_hline(buff, cx - y, cx + y, cy + x, mode);
				write_hline(buff, cx - y, cx + y, cy - x, mode);
				xch = 0;
			}
		}
		error += (y * 2) + 1;
		y++;
		if (error >= 0) {
			--x;
			xch    = 1;
			error -= x * 2;
		}
	}
	// Handle toggle mode.
	if (mode == 2) {
		write_hline(buff, cx - r, cx + r, cy, mode);
	}
}

/**
 * write_line: Draw a line of arbitrary angle.
 *
 * @param       buff    pointer to buffer to write in
 * @param       x0              first x coordinate
 * @param       y0              first y coordinate
 * @param       x1              second x coordinate
 * @param       y1              second y coordinate
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_line(uint8_t *buff, int x0, int y0, int x1, int y1, int mode)
{
	// Based on http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	int steep = abs(y1 - y0) > abs(x1 - x0);

	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}
	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}
	int deltax     = x1 - x0;
	int deltay = abs(y1 - y0);
	int error      = deltax / 2;
	int ystep;
	int y = y0;
	int x; // , lasty = y, stox = 0;
	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}
	for (x = x0; x < x1; x++) {
		if (steep) {
			write_pixel(buff, y, x, mode);
		} else {
			write_pixel(buff, x, y, mode);
		}
		error -= deltay;
		if (error < 0) {
			y     += ystep;
			error += deltax;
		}
	}
}

/**
 * write_line_lm: Draw a line of arbitrary angle.
 *
 * @param       x0              first x coordinate
 * @param       y0              first y coordinate
 * @param       x1              second x coordinate
 * @param       y1              second y coordinate
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 * @param       lmode   0 = clear, 1 = set, 2 = toggle
 */
void write_line_lm(int x0, int y0, int x1, int y1, int mmode, int lmode)
{
	write_line(draw_buffer_mask, x0, y0, x1, y1, mmode);
	write_line(draw_buffer_level, x0, y0, x1, y1, lmode);
}

/**
 * write_line_outlined: Draw a line of arbitrary angle, with an outline.
 *
 * @param       x0                      first x coordinate
 * @param       y0                      first y coordinate
 * @param       x1                      second x coordinate
 * @param       y1                      second y coordinate
 * @param       endcap0         0 = none, 1 = single pixel, 2 = full cap
 * @param       endcap1         0 = none, 1 = single pixel, 2 = full cap
 * @param       mode            0 = black outline, white body, 1 = white outline, black body
 * @param       mmode           0 = clear, 1 = set, 2 = toggle
 */
void write_line_outlined(int x0, int y0, int x1, int y1,
						 __attribute__((unused)) int endcap0, __attribute__((unused)) int endcap1,
						 int mode, int mmode)
{
	// Based on http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	// This could be improved for speed.
	int omode, imode;

	if (mode == 0) {
		omode = 0;
		imode = 1;
	} else {
		omode = 1;
		imode = 0;
	}
	int steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}
	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}
	int deltax     = x1 - x0;
	int deltay = abs(y1 - y0);
	int error      = deltax / 2;
	int ystep;
	int y = y0;
	int x;
	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}
	// Draw the outline.
	for (x = x0; x < x1; x++) {
		if (steep) {
			write_pixel_lm(y - 1, x, mmode, omode);
			write_pixel_lm(y + 1, x, mmode, omode);
			write_pixel_lm(y, x - 1, mmode, omode);
			write_pixel_lm(y, x + 1, mmode, omode);
		} else {
			write_pixel_lm(x - 1, y, mmode, omode);
			write_pixel_lm(x + 1, y, mmode, omode);
			write_pixel_lm(x, y - 1, mmode, omode);
			write_pixel_lm(x, y + 1, mmode, omode);
		}
		error -= deltay;
		if (error < 0) {
			y     += ystep;
			error += deltax;
		}
	}
	// Now draw the innards.
	error = deltax / 2;
	y     = y0;
	for (x = x0; x < x1; x++) {
		if (steep) {
			write_pixel_lm(y, x, mmode, imode);
		} else {
			write_pixel_lm(x, y, mmode, imode);
		}
		error -= deltay;
		if (error < 0) {
			y     += ystep;
			error += deltax;
		}
	}
}

/**
 * write_line_outlined_dashed: Draw a line of arbitrary angle, with an outline, potentially dashed.
 *
 * @param       x0              first x coordinate
 * @param       y0              first y coordinate
 * @param       x1              second x coordinate
 * @param       y1              second y coordinate
 * @param       endcap0         0 = none, 1 = single pixel, 2 = full cap
 * @param       endcap1         0 = none, 1 = single pixel, 2 = full cap
 * @param       mode            0 = black outline, white body, 1 = white outline, black body
 * @param       mmode           0 = clear, 1 = set, 2 = toggle
 * @param       dots			0 = not dashed, > 0 = # of set/unset dots for the dashed innards
 */
void write_line_outlined_dashed(int x0, int y0, int x1, int y1,
								__attribute__((unused)) int endcap0, __attribute__((unused)) int endcap1,
								int mode, int mmode, int dots)
{
	// Based on http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
	// This could be improved for speed.
	int omode, imode;

	if (mode == 0) {
		omode = 0;
		imode = 1;
	} else {
		omode = 1;
		imode = 0;
	}
	int steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}
	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}
	int deltax = x1 - x0;
	int deltay = abs(y1 - y0);
	int error  = deltax / 2;
	int ystep;
	int y = y0;
	int x;
	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}
	// Draw the outline.
	for (x = x0; x < x1; x++) {
		if (steep) {
			write_pixel_lm(y - 1, x, mmode, omode);
			write_pixel_lm(y + 1, x, mmode, omode);
			write_pixel_lm(y, x - 1, mmode, omode);
			write_pixel_lm(y, x + 1, mmode, omode);
		} else {
			write_pixel_lm(x - 1, y, mmode, omode);
			write_pixel_lm(x + 1, y, mmode, omode);
			write_pixel_lm(x, y - 1, mmode, omode);
			write_pixel_lm(x, y + 1, mmode, omode);
		}
		error -= deltay;
		if (error < 0) {
			y     += ystep;
			error += deltax;
		}
	}
	// Now draw the innards.
	error = deltax / 2;
	y     = y0;
	int dot_cnt = 0;
	int draw    = 1;
	for (x = x0; x < x1; x++) {
		if (dots && !(dot_cnt++ % dots)) {
			draw++;
		}
		if (draw % 2) {
			if (steep) {
				write_pixel_lm(y, x, mmode, imode);
			} else {
				write_pixel_lm(x, y, mmode, imode);
			}
		}
		error -= deltay;
		if (error < 0) {
			y     += ystep;
			error += deltax;
		}
	}
}

/**
 * write_word_misaligned: Write a misaligned word across two addresses
 * with an x offset.
 *
 * This allows for many pixels to be set in one write.
 *
 * @param       buff    buffer to write in
 * @param       word    word to write (16 bits)
 * @param       addr    address of first word
 * @param       xoff    x offset (0-15)
 * @param       mode    0 = clear, 1 = set, 2 = toggle
 */
void write_word_misaligned(uint8_t *buff, uint16_t word, unsigned int addr, unsigned int xoff, int mode)
{
	int16_t firstmask = word >> xoff;
	int16_t lastmask  = word << (16 - xoff);

	WRITE_WORD_MODE(buff, addr + 1, firstmask && 0x00ff, mode);
	WRITE_WORD_MODE(buff, addr, (firstmask & 0xff00) >> 8, mode);
	if (xoff > 0) {
		WRITE_WORD_MODE(buff, addr + 2, (lastmask & 0xff00) >> 8, mode);
	}
}

/**
 * write_word_misaligned_NAND: Write a misaligned word across two addresses
 * with an x offset, using a NAND mask.
 *
 * This allows for many pixels to be set in one write.
 *
 * @param       buff    buffer to write in
 * @param       word    word to write (16 bits)
 * @param       addr    address of first word
 * @param       xoff    x offset (0-15)
 *
 * This is identical to calling write_word_misaligned with a mode of 0 but
 * it doesn't go through a lot of switch logic which slows down text writing
 * a lot.
 */
void write_word_misaligned_NAND(uint8_t *buff, uint16_t word, unsigned int addr, unsigned int xoff)
{
	uint16_t firstmask = word >> xoff;
	uint16_t lastmask  = word << (16 - xoff);

	WRITE_WORD_NAND(buff, addr + 1, firstmask & 0x00ff);
	WRITE_WORD_NAND(buff, addr, (firstmask & 0xff00) >> 8);
	if (xoff > 0) {
		WRITE_WORD_NAND(buff, addr + 2, (lastmask & 0xff00) >> 8);
	}
}

/**
 * write_word_misaligned_OR: Write a misaligned word across two addresses
 * with an x offset, using an OR mask.
 *
 * This allows for many pixels to be set in one write.
 *
 * @param       buff    buffer to write in
 * @param       word    word to write (16 bits)
 * @param       addr    address of first word
 * @param       xoff    x offset (0-15)
 *
 * This is identical to calling write_word_misaligned with a mode of 1 but
 * it doesn't go through a lot of switch logic which slows down text writing
 * a lot.
 */
void write_word_misaligned_OR(uint8_t *buff, uint16_t word, unsigned int addr, unsigned int xoff)
{
	uint16_t firstmask = word >> xoff;
	uint16_t lastmask  = word << (16 - xoff);

	WRITE_WORD_OR(buff, addr + 1, firstmask & 0x00ff);
	WRITE_WORD_OR(buff, addr, (firstmask & 0xff00) >> 8);
	if (xoff > 0) {
		WRITE_WORD_OR(buff, addr + 2, (lastmask & 0xff00) >> 8);
	}
}

/**
 * write_word_misaligned_lm: Write a misaligned word across two
 * words, in both level and mask buffers. This is core to the text
 * writing routines.
 *
 * @param       buff    buffer to write in
 * @param       word    word to write (16 bits)
 * @param       addr    address of first word
 * @param       xoff    x offset (0-15)
 * @param       lmode   0 = clear, 1 = set, 2 = toggle
 * @param       mmode   0 = clear, 1 = set, 2 = toggle
 */
void write_word_misaligned_lm(uint16_t wordl, uint16_t wordm, unsigned int addr, unsigned int xoff, int lmode, int mmode)
{
	write_word_misaligned(draw_buffer_level, wordl, addr, xoff, lmode);
	write_word_misaligned(draw_buffer_mask, wordm, addr, xoff, mmode);
}

/**
 * fetch_font_info: Fetch font info structs.
 *
 * @param       ch              character
 * @param       font    font id
 */
int fetch_font_info(uint8_t ch, int font, struct FontEntry *font_info, char *lookup)
{
	// First locate the font struct.
	if ((unsigned int)font > SIZEOF_ARRAY(fonts)) {
		return 0; // font does not exist, exit.
	}
	// Load the font info; IDs are always sequential.
	*font_info = fonts[font];
	// Locate character in font lookup table. (If required.)
	if (lookup != NULL) {
		*lookup = font_info->lookup[ch];
		if (*lookup == 0xff) {
			return 0; // character doesn't exist, don't bother writing it.
		}
	}
	return 1;
}

/**
 * write_char16: Draw a character on the current draw buffer.
 *
 * @param       ch      character to write
 * @param       x       x coordinate (left)
 * @param       y       y coordinate (top)
 * @param       font    font to use
 */
void write_char16(char ch, int x, int y, int font)
{
	int yy, row, xshift;
	uint16_t and_mask, or_mask, levels;
	struct FontEntry font_info;

	fetch_font_info(0, font, &font_info, NULL);

	// check if char is partly out of boundary
	uint8_t partly_out = (x < GRAPHICS_LEFT) || (x + font_info.width > GRAPHICS_RIGHT) || (y < GRAPHICS_TOP) || (y + font_info.height > GRAPHICS_BOTTOM);
	// check if char is totally out of boundary, if so return
	if (partly_out && ((x + font_info.width < GRAPHICS_LEFT) || (x > GRAPHICS_RIGHT) || (y + font_info.height < GRAPHICS_TOP) || (y > GRAPHICS_BOTTOM))) {
		return;
	}

	// Compute starting address of character
	int addr = CALC_BUFF_ADDR(x, y);
	int wbit = CALC_BIT_IN_WORD(x);

	// If font only supports lowercase or uppercase, make the letter lowercase or uppercase
	// if (font_info.flags & FONT_LOWERCASE_ONLY) ch = tolower(ch);
	// if (font_info.flags & FONT_UPPERCASE_ONLY) ch = toupper(ch);

	// How wide is the character? We handle characters from 8 pixels up in this function
	if (font_info.width >= 8) {
		// Load data pointer.
		row    = ch * font_info.height;
		xshift = 16 - font_info.width;
		// We can write mask words easily.
		// Level bits are more complicated. We need to set or clear level bits, but only where the mask bit is set; otherwise, we need to leave them alone.
		// To do this, for each word, we construct an AND mask and an OR mask, and apply each individually.
		for (yy = y; yy < y + font_info.height; yy++) {
			if (!partly_out || ((x >= GRAPHICS_LEFT) && (x + font_info.width <= GRAPHICS_RIGHT) && (yy >= GRAPHICS_TOP) && (yy <= GRAPHICS_BOTTOM))) {
				if (font == 3) {
					// mask
					write_word_misaligned_OR(draw_buffer_mask, font_mask12x18[row] << xshift, addr, wbit);
					// level
					levels   = font_frame12x18[row];
					// if (!(flags & FONT_INVERT)) // data is normally inverted
					levels   = ~levels;
					or_mask  = font_mask12x18[row] << xshift;
					and_mask = (font_mask12x18[row] & levels) << xshift;
				} else {
					// mask
					write_word_misaligned_OR(draw_buffer_mask, font_mask8x10[row] << xshift, addr, wbit);
					// level
					levels   = font_frame8x10[row];
					// if (!(flags & FONT_INVERT)) // data is normally inverted
					levels   = ~levels;
					or_mask  = font_mask8x10[row] << xshift;
					and_mask = (font_mask8x10[row] & levels) << xshift;
				}
				write_word_misaligned_OR(draw_buffer_level, or_mask, addr, wbit);
				// If we're not bold write the AND mask.
				// if (!(flags & FONT_BOLD))
				write_word_misaligned_NAND(draw_buffer_level, and_mask, addr, wbit);
			}
			addr += BUFFER_WIDTH;
			row++;
		}
	}
}

/**
 * write_char: Draw a character on the current draw buffer.
 * Currently supports outlined characters and characters with a width of up to 8 pixels.
 *
 * @param       ch      character to write
 * @param       x       x coordinate (left)
 * @param       y       y coordinate (top)
 * @param       flags   flags to write with
 * @param       font    font to use
 */
void write_char(char ch, int x, int y, int flags, int font)
{
	int yy, row, xshift;
	uint16_t and_mask, or_mask, levels;
	struct FontEntry font_info;
	char lookup = 0;

	fetch_font_info(ch, font, &font_info, &lookup);

	// check if char is partly out of boundary
	uint8_t partly_out = (x < GRAPHICS_LEFT) || (x + font_info.width > GRAPHICS_RIGHT) || (y < GRAPHICS_TOP) || (y + font_info.height > GRAPHICS_BOTTOM);
	// check if char is totally out of boundary, if so return
	if (partly_out && ((x + font_info.width < GRAPHICS_LEFT) || (x > GRAPHICS_RIGHT) || (y + font_info.height < GRAPHICS_TOP) || (y > GRAPHICS_BOTTOM))) {
		return;
	}

	// Compute starting address of character
	unsigned int addr = CALC_BUFF_ADDR(x, y);
	unsigned int wbit = CALC_BIT_IN_WORD(x);

	// If font only supports lowercase or uppercase, make the letter lowercase or uppercase
	// if (font_info.flags & FONT_LOWERCASE_ONLY) ch = tolower(ch);
	// if (font_info.flags & FONT_UPPERCASE_ONLY) ch = toupper(ch);

	// How wide is the character? We handle characters up to 8 pixels in this function
	if (font_info.width <= 8) {
		// Load data pointer.
		row    = lookup * font_info.height * 2;
		xshift = 16 - font_info.width;
		// We can write mask words easily.
		// Level bits are more complicated. We need to set or clear level bits, but only where the mask bit is set; otherwise, we need to leave them alone.
		// To do this, for each word, we construct an AND mask and an OR mask, and apply each individually.
		for (yy = y; yy < y + font_info.height; yy++) {
			if (!partly_out || ((x >= GRAPHICS_LEFT) && (x + font_info.width <= GRAPHICS_RIGHT) && (yy >= GRAPHICS_TOP) && (yy <= GRAPHICS_BOTTOM))) {
				// mask
				write_word_misaligned_OR(draw_buffer_mask, font_info.data[row] << xshift, addr, wbit);
				// level
				levels = font_info.data[row + font_info.height];
				if (!(flags & FONT_INVERT)) { // data is normally inverted
					levels = ~levels;
				}
				or_mask  = font_info.data[row] << xshift;
				and_mask = (font_info.data[row] & levels) << xshift;
				write_word_misaligned_OR(draw_buffer_level, or_mask, addr, wbit);
				// If we're not bold write the AND mask.
				// if (!(flags & FONT_BOLD))
				write_word_misaligned_NAND(draw_buffer_level, and_mask, addr, wbit);
			}
			addr += BUFFER_WIDTH;
			row++;
		}
	}
}

/**
 * calc_text_dimensions: Calculate the dimensions of a
 * string in a given font. Supports new lines and
 * carriage returns in text.
 *
 * @param       str                     string to calculate dimensions of
 * @param       font_info       font info structure
 * @param       xs                      horizontal spacing
 * @param       ys                      vertical spacing
 * @param       dim                     return result: struct FontDimensions
 */
void calc_text_dimensions(char *str, struct FontEntry font, int xs, int ys, struct FontDimensions *dim)
{
	int max_length = 0, line_length = 0, lines = 1;

	while (*str != 0) {
		line_length++;
		if (*str == '\n' || *str == '\r') {
			if (line_length > max_length) {
				max_length = line_length;
			}
			line_length = 0;
			lines++;
		}
		str++;
	}
	if (line_length > max_length) {
		max_length = line_length;
	}
	dim->width  = max_length * (font.width + xs);
	dim->height = lines * (font.height + ys);
}

/**
 * write_string: Draw a string on the screen with certain
 * alignment parameters.
 *
 * @param       str             string to write
 * @param       x               x coordinate
 * @param       y               y coordinate
 * @param       xs              horizontal spacing
 * @param       ys              horizontal spacing
 * @param       va              vertical align
 * @param       ha              horizontal align
 * @param       flags   flags (passed to write_char)
 * @param       font    font
 */
void write_string(char *str, int x, int y, int xs, int ys, int va, int ha, int flags, int font)
{
	int xx = 0, yy = 0, xx_original = 0;
	struct FontEntry font_info;
	struct FontDimensions dim;

	// Determine font info and dimensions/position of the string.
	fetch_font_info(0, font, &font_info, NULL);
	calc_text_dimensions(str, font_info, xs, ys, &dim);
	switch (va) {
	case TEXT_VA_TOP:
		yy = y;
		break;
	case TEXT_VA_MIDDLE:
		yy = y - (dim.height / 2);
		break;
	case TEXT_VA_BOTTOM:
		yy = y - dim.height;
		break;
	}
	switch (ha) {
	case TEXT_HA_LEFT:
		xx = x;
		break;
	case TEXT_HA_CENTER:
		xx = x - (dim.width / 2);
		break;
	case TEXT_HA_RIGHT:
		xx = x - dim.width;
		break;
	}
	// Then write each character.
	xx_original = xx;
	while (*str != 0) {
		if (*str == '\n' || *str == '\r') {
			yy += ys + font_info.height;
			xx  = xx_original;
		} else {
			if (xx >= 0 && xx < GRAPHICS_WIDTH_REAL) {
				if (font_info.id < 2) {
					write_char(*str, xx, yy, flags, font);
				} else {
					write_char16(*str, xx, yy, font);
				}
			}
			xx += font_info.width + xs;
		}
		str++;
	}
}

void drawBattery(uint16_t x, uint16_t y, uint8_t battery, uint16_t size)
{
	int i = 0;
	int batteryLines;

	write_rectangle_outlined((x) - 1, (y) - 1 + 2, size, size * 3, 0, 1);
	write_vline_lm((x) - 1 + (size / 2 + size / 4) + 1, (y) - 2, (y) - 1 + 1, 0, 1);
	write_vline_lm((x) - 1 + (size / 2 - size / 4) - 1, (y) - 2, (y) - 1 + 1, 0, 1);
	write_hline_lm((x) - 1 + (size / 2 - size / 4), (x) - 1 + (size / 2 + size / 4), (y) - 2, 0, 1);
	write_hline_lm((x) - 1 + (size / 2 - size / 4), (x) - 1 + (size / 2 + size / 4), (y) - 1, 1, 1);
	write_hline_lm((x) - 1 + (size / 2 - size / 4), (x) - 1 + (size / 2 + size / 4), (y) - 1 + 1, 1, 1);

	batteryLines = battery * (size * 3 - 2) / 100;
	for (i = 0; i < batteryLines; i++) {
		write_hline_lm((x) - 1, (x) - 1 + size, (y) - 1 + size * 3 - i, 1, 1);
	}
}

/**
 * hud_draw_vertical_scale: Draw a vertical scale.
 *
 * @param       v                   value to display as an integer
 * @param       range               range about value to display (+/- range/2 each direction)
 * @param       halign              horizontal alignment: -1 = left, +1 = right.
 * @param       x                   x displacement
 * @param       y                   y displacement
 * @param       height              height of scale
 * @param       mintick_step        how often a minor tick is shown
 * @param       majtick_step        how often a major tick is shown
 * @param       mintick_len         minor tick length
 * @param       majtick_len         major tick length
 * @param       boundtick_len       boundary tick length
 * @param       max_val             maximum expected value (used to compute size of arrow ticker)
 * @param       flags               special flags (see hud.h.)
 */
// #define VERTICAL_SCALE_BRUTE_FORCE_BLANK_OUT
#define VERTICAL_SCALE_FILLED_NUMBER
void hud_draw_vertical_scale(int v, int range, int halign, int x, int y, int height, int mintick_step, int majtick_step, int mintick_len, int majtick_len,
							 int boundtick_len, __attribute__((unused)) int max_val, int flags)
{
	char temp[15];
	struct FontEntry font_info;
	struct FontDimensions dim;
	// Compute the position of the elements.
	int majtick_start = 0, majtick_end = 0, mintick_start = 0, mintick_end = 0, boundtick_start = 0, boundtick_end = 0;

	majtick_start   = x;
	mintick_start   = x;
	boundtick_start = x;
	if (halign == -1) {
		majtick_end     = x + majtick_len;
		mintick_end     = x + mintick_len;
		boundtick_end   = x + boundtick_len;
	} else if (halign == +1) {
		majtick_end     = x - majtick_len;
		mintick_end     = x - mintick_len;
		boundtick_end   = x - boundtick_len;
	}
	// Retrieve width of large font (font #0); from this calculate the x spacing.
	fetch_font_info(0, 0, &font_info, NULL);
	int arrow_len      = (font_info.height / 2) + 1;
	int text_x_spacing = (font_info.width / 2);
	int max_text_y     = 0, text_length = 0;
	int small_font_char_width = font_info.width + 1; // +1 for horizontal spacing = 1
	// For -(range / 2) to +(range / 2), draw the scale.
	int range_2 = range / 2; // , height_2 = height / 2;
	int r = 0, rr = 0, rv = 0, ys = 0, style = 0; // calc_ys = 0,
	// Iterate through each step.
	for (r = -range_2; r <= +range_2; r++) {
		style = 0;
		rr    = r + range_2 - v; // normalise range for modulo, subtract value to move ticker tape
		rv    = -rr + range_2; // for number display
		if (flags & HUD_VSCALE_FLAG_NO_NEGATIVE) {
			rr += majtick_step / 2;
		}
		if (rr % majtick_step == 0) {
			style = 1; // major tick
		} else if (rr % mintick_step == 0) {
			style = 2; // minor tick
		} else {
			style = 0;
		}
		if (flags & HUD_VSCALE_FLAG_NO_NEGATIVE && rv < 0) {
			continue;
		}
		if (style) {
			// Calculate y position.
			ys = ((long int)(r * height) / (long int)range) + y;
			// Depending on style, draw a minor or a major tick.
			if (style == 1) {
				write_hline_outlined(majtick_start, majtick_end, ys, 2, 2, 0, 1);
				memset(temp, ' ', 10);
				sprintf(temp, "%d", rv);
				text_length = (strlen(temp) + 1) * small_font_char_width; // add 1 for margin
				if (text_length > max_text_y) {
					max_text_y = text_length;
				}
				if (halign == -1) {
					write_string(temp, majtick_end + text_x_spacing + 1, ys, 1, 0, TEXT_VA_MIDDLE, TEXT_HA_LEFT, 0, 1);
				} else {
					write_string(temp, majtick_end - text_x_spacing + 1, ys, 1, 0, TEXT_VA_MIDDLE, TEXT_HA_RIGHT, 0, 1);
				}
			} else if (style == 2) {
				write_hline_outlined(mintick_start, mintick_end, ys, 2, 2, 0, 1);
			}
		}
	}
	// Generate the string for the value, as well as calculating its dimensions.
	memset(temp, ' ', 10);
	// my_itoa(v, temp);
	sprintf(temp, "%d", v);
	// TODO: add auto-sizing.
	calc_text_dimensions(temp, font_info, 1, 0, &dim);
	int xx = 0, i = 0;
	if (halign == -1) {
		xx = majtick_end + text_x_spacing;
	} else {
		xx = majtick_end - text_x_spacing;
	}
	y++;
	// Draw an arrow from the number to the point.
	for (i = 0; i < arrow_len; i++) {
		if (halign == -1) {
			write_pixel_lm(xx - arrow_len + i, y - i - 1, 1, 1);
			write_pixel_lm(xx - arrow_len + i, y + i - 1, 1, 1);
#ifdef VERTICAL_SCALE_FILLED_NUMBER
			write_hline_lm(xx + dim.width - 1, xx - arrow_len + i + 1, y - i - 1, 0, 1);
			write_hline_lm(xx + dim.width - 1, xx - arrow_len + i + 1, y + i - 1, 0, 1);
#else
			write_hline_lm(xx + dim.width - 1, xx - arrow_len + i + 1, y - i - 1, 0, 0);
			write_hline_lm(xx + dim.width - 1, xx - arrow_len + i + 1, y + i - 1, 0, 0);
#endif
		} else {
			write_pixel_lm(xx + arrow_len - i, y - i - 1, 1, 1);
			write_pixel_lm(xx + arrow_len - i, y + i - 1, 1, 1);
#ifdef VERTICAL_SCALE_FILLED_NUMBER
			write_hline_lm(xx - dim.width - 1, xx + arrow_len - i - 1, y - i - 1, 0, 1);
			write_hline_lm(xx - dim.width - 1, xx + arrow_len - i - 1, y + i - 1, 0, 1);
#else
			write_hline_lm(xx - dim.width - 1, xx + arrow_len - i - 1, y - i - 1, 0, 0);
			write_hline_lm(xx - dim.width - 1, xx + arrow_len - i - 1, y + i - 1, 0, 0);
#endif
		}
	}
	if (halign == -1) {
		write_hline_lm(xx, xx + dim.width - 1, y - arrow_len, 1, 1);
		write_hline_lm(xx, xx + dim.width - 1, y + arrow_len - 2, 1, 1);
		write_vline_lm(xx + dim.width - 1, y - arrow_len, y + arrow_len - 2, 1, 1);
	} else {
		write_hline_lm(xx, xx - dim.width - 1, y - arrow_len, 1, 1);
		write_hline_lm(xx, xx - dim.width - 1, y + arrow_len - 2, 1, 1);
		write_vline_lm(xx - dim.width - 1, y - arrow_len, y + arrow_len - 2, 1, 1);
	}
	// Draw the text.
	if (halign == -1) {
		write_string(temp, xx, y, 1, 0, TEXT_VA_MIDDLE, TEXT_HA_LEFT, 0, 0);
	} else {
		write_string(temp, xx, y, 1, 0, TEXT_VA_MIDDLE, TEXT_HA_RIGHT, 0, 0);
	}
#ifdef VERTICAL_SCALE_BRUTE_FORCE_BLANK_OUT
	// This is a bad brute force method destuctive to other things that maybe drawn underneath like e.g. the artificial horizon:
	// Then, add a slow cut off on the edges, so the text doesn't sharply
	// disappear. We simply clear the areas above and below the ticker, and we
	// use little markers on the edges.
	if (halign == -1) {
		write_filled_rectangle_lm(majtick_end + text_x_spacing, y + (height / 2) - (font_info.height / 2), max_text_y - boundtick_start, font_info.height, 0, 0);
		write_filled_rectangle_lm(majtick_end + text_x_spacing, y - (height / 2) - (font_info.height / 2), max_text_y - boundtick_start, font_info.height, 0, 0);
	} else {
		write_filled_rectangle_lm(majtick_end - text_x_spacing - max_text_y, y + (height / 2) - (font_info.height / 2), max_text_y, font_info.height, 0, 0);
		write_filled_rectangle_lm(majtick_end - text_x_spacing - max_text_y, y - (height / 2) - (font_info.height / 2), max_text_y, font_info.height, 0, 0);
	}
#endif
	y--;
	write_hline_outlined(boundtick_start, boundtick_end, y + (height / 2), 2, 2, 0, 1);
	write_hline_outlined(boundtick_start, boundtick_end, y - (height / 2), 2, 2, 0, 1);
}

/**
 * hud_draw_compass: Draw a compass.
 *
 * @param       v               value for the compass
 * @param       range           range about value to display (+/- range/2 each direction)
 * @param       width           length in pixels
 * @param       x               x displacement
 * @param       y               y displacement
 * @param       mintick_step    how often a minor tick is shown
 * @param       majtick_step    how often a major tick (heading "xx") is shown
 * @param       mintick_len     minor tick length
 * @param       majtick_len     major tick length
 * @param       flags           special flags (see hud.h.)
 */
#define COMPASS_SMALL_NUMBER
// #define COMPASS_FILLED_NUMBER
void hud_draw_linear_compass(int v, int range, int width, int x, int y, int mintick_step, int majtick_step, int mintick_len, int majtick_len, __attribute__((unused)) int flags)
{
	v %= 360; // wrap, just in case.
	struct FontEntry font_info;
	int majtick_start = 0, majtick_end = 0, mintick_start = 0, mintick_end = 0, textoffset = 0;
	char headingstr[4];
	majtick_start = y;
	majtick_end   = y - majtick_len;
	mintick_start = y;
	mintick_end   = y - mintick_len;
	textoffset    = 8;
	int r, style, rr, xs; // rv,
	int range_2 = range / 2;
	for (r = -range_2; r <= +range_2; r++) {
		style = 0;
		rr    = (v + r + 360) % 360; // normalise range for modulo, add to move compass track
		// rv = -rr + range_2; // for number display
		if (rr % majtick_step == 0) {
			style = 1; // major tick
		} else if (rr % mintick_step == 0) {
			style = 2; // minor tick
		}
		if (style) {
			// Calculate x position.
			xs = ((long int)(r * width) / (long int)range) + x;
			// Draw it.
			if (style == 1) {
				write_vline_outlined(xs, majtick_start, majtick_end, 2, 2, 0, 1);
				// Draw heading above this tick.
				// If it's not one of north, south, east, west, draw the heading.
				// Otherwise, draw one of the identifiers.
				if (rr % 90 != 0) {
					// We abbreviate heading to two digits. This has the side effect of being easy to compute.
					headingstr[0] = '0' + (rr / 100);
					headingstr[1] = '0' + ((rr / 10) % 10);
					headingstr[2] = 0;
					headingstr[3] = 0; // nul to terminate
				} else {
					switch (rr) {
					case 0:
						headingstr[0] = 'N';
						break;
					case 90:
						headingstr[0] = 'E';
						break;
					case 180:
						headingstr[0] = 'S';
						break;
					case 270:
						headingstr[0] = 'W';
						break;
					}
					headingstr[1] = 0;
					headingstr[2] = 0;
					headingstr[3] = 0;
				}
				// +1 fudge...!
				write_string(headingstr, xs + 1, majtick_start + textoffset, 1, 0, TEXT_VA_MIDDLE, TEXT_HA_CENTER, 0, 1);
			} else if (style == 2) {
				write_vline_outlined(xs, mintick_start, mintick_end, 2, 2, 0, 1);
			}
		}
	}
	// Then, draw a rectangle with the present heading in it.
	// We want to cover up any other markers on the bottom.
	// First compute font size.
	headingstr[0] = '0' + (v / 100);
	headingstr[1] = '0' + ((v / 10) % 10);
	headingstr[2] = '0' + (v % 10);
	headingstr[3] = 0;
	fetch_font_info(0, 3, &font_info, NULL);
#ifdef COMPASS_SMALL_NUMBER
	int rect_width = font_info.width * 3;
#ifdef COMPASS_FILLED_NUMBER
	write_filled_rectangle_lm(x - (rect_width / 2), majtick_start - 7, rect_width, font_info.height, 0, 1);
#else
	write_filled_rectangle_lm(x - (rect_width / 2), majtick_start - 7, rect_width, font_info.height, 0, 0);
#endif
	write_rectangle_outlined(x - (rect_width / 2), majtick_start - 7, rect_width, font_info.height, 0, 1);
	write_string(headingstr, x + 1, majtick_start + textoffset - 5, 0, 0, TEXT_VA_MIDDLE, TEXT_HA_CENTER, 1, 0);
#else
	int rect_width = (font_info.width + 1) * 3 + 2;
#ifdef COMPASS_FILLED_NUMBER
	write_filled_rectangle_lm(x - (rect_width / 2), majtick_start + 2, rect_width, font_info.height + 2, 0, 1);
#else
	write_filled_rectangle_lm(x - (rect_width / 2), majtick_start + 2, rect_width, font_info.height + 2, 0, 0);
#endif
	write_rectangle_outlined(x - (rect_width / 2), majtick_start + 2, rect_width, font_info.height + 2, 0, 1);
	write_string(headingstr, x + 1, majtick_start + textoffset + 2, 0, 0, TEXT_VA_MIDDLE, TEXT_HA_CENTER, 1, 3);
#endif
}


#define CENTER_BODY       3
#define CENTER_WING       7
#define CENTER_RUDDER     5
void simple_artifical_horizon(float roll, float pitch, int16_t x, int16_t y, int16_t width, int16_t height,
							  int8_t max_pitch)
{
	float sin_roll;
	float cos_roll;
	int16_t d_x; // delta x
	int16_t d_y; // delta y

	int16_t pp_x; // pitch point x
	int16_t pp_y; // pitch point y

	if (roll > 0) {
		sin_roll    = sin_lookup_deg(roll);
		cos_roll    = cos_lookup_deg(roll);
	}
	else {
		sin_roll    = -1 * sin_lookup_deg(-1 * roll);
		cos_roll    = cos_lookup_deg(-1 * roll);
	}

	// roll to pitch transformation
	pp_x        = x * (1 + (sin_roll * pitch) / (float)max_pitch);
	pp_y        = y * (1 + (cos_roll * pitch) / (float)max_pitch);

	// main horizon
	d_x = cos_roll * width / 2;
	d_y = sin_roll * width / 2;
	write_line_outlined_dashed(pp_x - d_x, pp_y + d_y, pp_x + d_x, pp_y - d_y, 2, 2, 0, 1, 0);

	// center mark
	//write_circle_outlined(x, y, CENTER_BODY, 0, 0, 0, 1);
	write_line_outlined(x - CENTER_WING - CENTER_BODY, y, x - CENTER_BODY, y, 2, 0, 0, 1);
	write_line_outlined(x + 1 + CENTER_BODY, y, x + 1 + CENTER_BODY + CENTER_WING, y, 0, 2, 0, 1);
	write_line_outlined(x, y - CENTER_RUDDER - CENTER_BODY, x, y - CENTER_BODY, 2, 0, 0, 1);
}

void draw_flight_mode(int x, int y, int xs, int ys, int va, int ha, int flags, int font)
{
	uint8_t mode;
	FlightStatusFlightModeGet(&mode);

	switch (mode)
	{
		case FLIGHTSTATUS_FLIGHTMODE_MANUAL:
			write_string("MAN", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_ACRO:
			write_string("ACRO", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_LEVELING:
			write_string("LEVEL", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_VIRTUALBAR:
			write_string("VBAR", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_STABILIZED1:
			write_string("ST1", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_STABILIZED2:
			write_string("ST2", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_STABILIZED3:
			write_string("ST3", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_AUTOTUNE:
			write_string("TUNE", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_ALTITUDEHOLD:
			write_string("AHLD", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_VELOCITYCONTROL:
			write_string("VCNT", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_POSITIONHOLD:
			write_string("PHLD", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_RETURNTOHOME:
			write_string("RTH", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_PATHPLANNER:
			write_string("PLAN", x, y, xs, ys, va, ha, flags, font);
			break;
		case FLIGHTSTATUS_FLIGHTMODE_TABLETCONTROL:
			write_string("TAB", x, y, xs, ys, va, ha, flags, font);
			break;
	}
}

const uint8_t ALL_ALRARMS[] = {SYSTEMALARMS_ALARM_OUTOFMEMORY,
								SYSTEMALARMS_ALARM_CPUOVERLOAD,
								SYSTEMALARMS_ALARM_STACKOVERFLOW,
								SYSTEMALARMS_ALARM_SYSTEMCONFIGURATION,
								SYSTEMALARMS_ALARM_EVENTSYSTEM,
//								SYSTEMALARMS_ALARM_TELEMETRY,
								SYSTEMALARMS_ALARM_MANUALCONTROL,
								SYSTEMALARMS_ALARM_ACTUATOR,
								SYSTEMALARMS_ALARM_ATTITUDE,
								SYSTEMALARMS_ALARM_SENSORS,
								SYSTEMALARMS_ALARM_STABILIZATION,
								SYSTEMALARMS_ALARM_PATHFOLLOWER,
								SYSTEMALARMS_ALARM_PATHPLANNER,
								SYSTEMALARMS_ALARM_BATTERY,
								SYSTEMALARMS_ALARM_FLIGHTTIME,
								SYSTEMALARMS_ALARM_I2C,
								SYSTEMALARMS_ALARM_GPS,
								SYSTEMALARMS_ALARM_ALTITUDEHOLD,
								SYSTEMALARMS_ALARM_BOOTFAULT};

const char * ALL_ALRARM_NAMES[] = {"MEMORY",
									"CPU",
									"STACK",
									"CONFIG",
									"EVENT",
//									"TELEMETRY",
									"MANUAL",
									"ACTUATOR",
									"ATTITUDE",
									"SENSORS",
									"STAB",
									"PATH-F",
									"PATH-P",
									"BATTERY",
									"TIME",
									"I2C",
									"GPS",
									"A-HOLD",
									"BOOT"};

void draw_alarms(int x, int y, int xs, int ys, int va, int ha, int flags, int font)
{
	uint8_t str_pos = 0;
	uint8_t this_len;
	char temp[100]  = { 0 };
	SystemAlarmsData alarm;

	SystemAlarmsGet(&alarm);

	for (uint8_t pos = 0; pos < sizeof(ALL_ALRARMS); pos++)
	{
		if ((alarm.Alarm[ALL_ALRARMS[pos]] == SYSTEMALARMS_ALARM_WARNING) ||
			(alarm.Alarm[ALL_ALRARMS[pos]] == SYSTEMALARMS_ALARM_ERROR) ||
			(alarm.Alarm[ALL_ALRARMS[pos]] == SYSTEMALARMS_ALARM_CRITICAL)){
			this_len = strlen(ALL_ALRARM_NAMES[pos]);
			if (str_pos + this_len + 2 >= sizeof(temp))
				break;

			if ((alarm.Alarm[ALL_ALRARMS[pos]] != SYSTEMALARMS_ALARM_WARNING)
				&& (frame_counter % BLINK_INTERVAL_FRAMES < BLINK_INTERVAL_FRAMES / 2)){
				// for alarms, we blink
				this_len += 1;
				while (this_len > 0){
					temp[str_pos++] = ' ';
					this_len--;
				}
				continue;
			}

			memcpy((void*)&temp[str_pos], (void*)ALL_ALRARM_NAMES[pos], this_len);
			str_pos += this_len;
			temp[str_pos] = ' ';
			str_pos += 1;
		}
	}
	if (str_pos > 0){
		temp[str_pos] = '\0';
		write_string(temp, x, y, xs, ys, va, ha, flags, font);
	}
}

void introGraphics(int16_t x, int16_t y)
{
	/* logo */
	draw_image(x - image_brain.width, y - image_brain.height / 2, &image_brain);
	draw_image(x + 50, y - image_tau.height / 2, &image_tau);
}

void introText(int16_t x, int16_t y)
{
	write_string("Brain FPV Flight Controller", x, y - 10, 0, 0, TEXT_VA_TOP, TEXT_HA_CENTER, 0, 3);
	write_string("Tau Labs", x, y + 10, 0, 0, TEXT_VA_TOP, TEXT_HA_CENTER, 0, 3);
}

void showVideoType(int16_t x, int16_t y)
{
	if (PIOS_Video_GetType() == VIDEO_TYPE_NTSC) {
		write_string("NTSC", x, y, 0, 0, TEXT_VA_TOP, TEXT_HA_CENTER, 0, 3);
	} else {
		write_string("PAL", x, y, 0, 0, TEXT_VA_TOP, TEXT_HA_CENTER, 0, 3);
	}
}

char digits[10] = "0123456789";
int utoa(unsigned int i, char *output, size_t olen)
{
	size_t sublen;
	if (olen < 2)      /* need room for at least 1 digit + '\0' */
		return 0;
	if (i < 10) {        /* base case */
		output[0] = digits[i];
		sublen = 0;
	} else {
		/*
						generate leading digits by recursive call;
						low order digit is then placed at end of array
				*/
		sublen = utoa(i/10, output, olen-1);
		if (sublen == 0)
			return 0;       /* propigate error */
		output[sublen] = digits[i % 10];
	}
	output[++sublen] = '\0';
	return sublen;
}


void updateGraphics()
{
	char temp[100] = { 0 };
	float tmp, tmp2, tmp3;
	int tmp_int1, tmp_int2;

	// CPU
	uint8_t cpu;
	SystemStatsCPULoadGet(&cpu);
	sprintf(temp, "CPU:%2d", cpu);
	write_string(temp, 5, GRAPHICS_BOTTOM - 20, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// Time
	uint32_t time;
	SystemStatsFlightTimeGet(&time);
	tmp_int1 = time / 60000;
	tmp_int2 = (time / 1000) - 60 * tmp_int1;
	sprintf(temp, "%02d:%02d", (int)tmp_int1, (int)tmp_int2);
	write_string(temp, GRAPHICS_RIGHT - 60, 40, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// Flight Mode
	draw_flight_mode(20, 10, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// Alarms
	draw_alarms(5, GRAPHICS_BOTTOM - 30, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// Arming
	uint8_t arm;
	FlightStatusArmedGet(&arm);
	if (arm != FLIGHTSTATUS_ARMED_DISARMED)
		write_string("ARMED", 20, 25, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// RSSI
	int16_t rssi;
	ManualControlCommandRssiGet(&rssi);
	sprintf(temp, "RSSI:%3d", rssi);
	write_string(temp, 55, GRAPHICS_BOTTOM - 20, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// Artificial horizon
	float roll, pitch;
	AttitudeActualRollGet(&roll);
	AttitudeActualPitchGet(&pitch);

	simple_artifical_horizon(roll, pitch, GRAPHICS_X_MIDDLE, GRAPHICS_Y_MIDDLE, 150, 150, 30);

	// Compass
	AttitudeActualYawGet(&tmp);
	if (tmp < 0)
		tmp += 360;
	hud_draw_linear_compass(tmp, 120, 180, GRAPHICS_X_MIDDLE, 20, 15, 30, 5, 8, 0);

	// Altitude
	PositionActualDownGet(&tmp);
	hud_draw_vertical_scale(-1 * tmp * convert_distance, 100, 1, GRAPHICS_RIGHT - 5, GRAPHICS_Y_MIDDLE, 120, 10, 20, 5, 8, 11, 10000, 0);

	// Speed
	VelocityActualNorthGet(&tmp);
	VelocityActualEastGet(&tmp2);
	VelocityActualDownGet(&tmp3);
	tmp = sqrt(tmp * tmp + tmp2 * tmp2 + tmp3 * tmp3);
	hud_draw_vertical_scale(tmp * convert_speed, 30, -1, 5, GRAPHICS_Y_MIDDLE, 120, 10, 20, 5, 8, 11, 100, 0);

	// Home distance and direction
	PositionActualNorthGet(&tmp);
	PositionActualEastGet(&tmp2);
	tmp3 = (float)sqrt(tmp * tmp + tmp2 * tmp2) * convert_distance; // distance

	tmp = atan2f(tmp2, tmp) * RAD2DEG;    // direction relative to position
	if (tmp < 0)
		tmp += 360;
	sprintf(temp, "Home: %0.1f %0.0fdeg", (double)tmp3, (double)tmp);
	write_string(temp, 5, GRAPHICS_BOTTOM - 40, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);

	// GPS Location
	if (module_state[MODULESETTINGS_ADMINSTATE_GPS] == MODULESETTINGS_ADMINSTATE_ENABLED) {
		GPSPositionData gps_data;
		GPSPositionGet(&gps_data);

		sprintf(temp,"Fix:%d Sats:%d Lat:%0.7f Lon:%0.7f",(int)gps_data.Status,
				(int)gps_data.Satellites, (double)gps_data.Latitude / 10000000.0,
				(double)gps_data.Longitude / 10000000.0);
		write_string(temp, 5, GRAPHICS_BOTTOM - 10, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
	}

	// Battery
	if (module_state[MODULESETTINGS_ADMINSTATE_BATTERY] == MODULESETTINGS_ADMINSTATE_ENABLED) {
		FlightBatteryStateVoltageGet(&tmp);
		sprintf(temp, "%0.1fV", (double)tmp);
		write_string(temp, GRAPHICS_RIGHT - 60, 10, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
		FlightBatteryStateCurrentGet(&tmp);
		sprintf(temp, "%0.1fA", (double)tmp);
		write_string(temp, GRAPHICS_RIGHT - 60, 20, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
		FlightBatteryStateConsumedEnergyGet(&tmp);
		sprintf(temp, "%0.0fmAh", (double)tmp);
		write_string(temp, GRAPHICS_RIGHT - 60, 30, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
	}

	//drawBox(0, 0, GRAPHICS_RIGHT, GRAPHICS_BOTTOM);

#ifdef DEBUG_TIMING
	// show in time
	sprintf(temp, "T.draw: %2dms", in_time);
	write_string(temp, 120, GRAPHICS_BOTTOM -20, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
#endif
}

void render_user_page(uint8_t test, float test2)
{
	char temp[100] = { 0 };
	sprintf(temp, "%d %0.3f", test, (double)test2);
	write_string(temp, GRAPHICS_X_MIDDLE, GRAPHICS_Y_MIDDLE, 0, 0, TEXT_VA_TOP, TEXT_HA_LEFT, 0, 2);
}


static void OnScreenSettingsUpdatedCb(UAVObjEvent * ev)
{
	osd_settings_updated = true;
}


/**
 * Start the osd module
 */
int32_t OnScreenDisplayStart(void)
{
	if (module_enabled)
	{
		vSemaphoreCreateBinary(onScreenDisplaySemaphore);
		xTaskCreate(onScreenDisplayTask, (signed char *)"OnScreenDisplay", STACK_SIZE_BYTES / 4, NULL, TASK_PRIORITY, &onScreenDisplayTaskHandle);
		TaskMonitorAdd(TASKINFO_RUNNING_ONSCREENDISPLAY, onScreenDisplayTaskHandle);
#if defined(PIOS_INCLUDE_WDG) && defined(OSD_USE_WDG)
		PIOS_WDG_RegisterFlag(PIOS_WDG_OSDGEN);
#endif
		return 0;
	}
	return -1;
}


/**
 * Initialize the osd module
 */
int32_t OnScreenDisplayInitialize(void)
{
	uint8_t osd_state;
	uint16_t num_instances;

	ModuleSettingsAdminStateGet(module_state);

	OnScreenDisplayPageSettingsInitialize();

	//num_instances = OnScreenDisplayPageSettingsGetNumInstances();
	//for (int ii=0; ii < 6- num_instances; ii++) {
	//	OnScreenDisplayPageSettingsCreateInstance();
	//}

	OnScreenDisplaySettingsOSDEnabledGet(&osd_state);
	if (osd_state == ONSCREENDISPLAYSETTINGS_OSDENABLED_ENABLED) {
		module_enabled = true;
	} else {
		module_enabled = false;
		return 0;
	}

	/* Register callbacks for modified settings */
	OnScreenDisplaySettingsConnectCallback(OnScreenSettingsUpdatedCb);
	return 0;
}


MODULE_INITCALL(OnScreenDisplayInitialize, OnScreenDisplayStart);

/**
 * Main osd task. It does not return.
 */
#define BLANK_TIME 3000
#define INTRO_TIME 5000
static void onScreenDisplayTask(__attribute__((unused)) void *parameters)
{
	AccessoryDesiredData accessory;
	OnScreenDisplaySettingsData osd_settings;
	uint8_t current_page = 0;


	// blank
	while (xTaskGetTickCount() <= BLANK_TIME) {
		if (xSemaphoreTake(onScreenDisplaySemaphore, LONG_TIME) == pdTRUE) {
			clearGraphics();
		}
	}

	// intro
	while (xTaskGetTickCount() <= BLANK_TIME + INTRO_TIME) {
		if (xSemaphoreTake(onScreenDisplaySemaphore, LONG_TIME) == pdTRUE) {
			clearGraphics();
			if (PIOS_Video_GetType() == VIDEO_TYPE_NTSC) {
				introGraphics(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM / 2 - 20);
				introText(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM - 70);
				showVideoType(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM - 45);
			} else {
				introGraphics(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM / 2 - 30);
				introText(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM - 90);
				showVideoType(GRAPHICS_RIGHT / 2, GRAPHICS_BOTTOM - 60);
			}
		}
	}

	while (1) {
		if (xSemaphoreTake(onScreenDisplaySemaphore, LONG_TIME) == pdTRUE) {
#ifdef DEBUG_TIMING
			in_ticks = xTaskGetTickCount();
			out_time = in_ticks - out_ticks;
#endif
			if (osd_settings_updated) {
				OnScreenDisplaySettingsGet(&osd_settings);
				PIOS_Video_SetLevels(osd_settings.PALBlack, osd_settings.PALWhite,
									 osd_settings.NTSCBlack, osd_settings.NTSCWhite);

				PIOS_Video_SetXOffset(osd_settings.XOffset);
				PIOS_Video_SetYOffset(osd_settings.YOffset);

				if (osd_settings.Units == ONSCREENDISPLAYSETTINGS_UNITS_IMPERIAL){
					convert_distance = M_TO_FEET;
					convert_speed = MS_TO_MPH;
				}
				else{
					convert_distance = 1.0f;
					convert_speed = MS_TO_KMH;
				}

				osd_settings_updated = false;
			}

			if (frame_counter % 10 == 0) {
				// determine current page to use
				AccessoryDesiredInstGet(osd_settings.PageSwitch, &accessory);
				current_page = (uint8_t)roundf(((accessory.AccessoryVal + 1.0f) / 2.0f) * (osd_settings.NumPages - 1));
				current_page = osd_settings.PageConfig[current_page];
			}

			clearGraphics();
			updateGraphics();
//			switch (current_page) {
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM1:
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM2:
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM3:
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM4:
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM5:
//				case ONSCREENDISPLAYSETTINGS_PAGECONFIG_CUSTOM6:
//					render_user_page(current_page, accessory.AccessoryVal);
//					break;
//			}

			frame_counter++;
#ifdef DEBUG_TIMING
			out_ticks = xTaskGetTickCount();
			in_time   = out_ticks - in_ticks;
#endif
		}
	}
}
