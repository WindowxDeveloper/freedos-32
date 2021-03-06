/* DPMI Driver for FD32: int 0x33 services
 * by Hanzac Chen
 *
 * This is free software; see GPL.txt
 */
 
#include <ll/i386/hw-data.h>
#include <ll/i386/mem.h>
#include <ll/i386/x-bios.h>
#include <ll/i386/error.h>
#include <devices.h>
#include <errno.h>
#include <logger.h>
#include "rmint.h"
#include <mouse.h>


#ifdef __INT33_DEBUG__
#define LOG_PRINTF(s, ...) fd32_log_printf("[MOUSE BIOS] "s, ## __VA_ARGS__)
#else
#define LOG_PRINTF(s, ...)
#endif


/* TODO: Make a common mouse header, define and put the request structures there */
typedef struct fd32_mouse_getbutton
{
	WORD button;
	WORD count;
	WORD x;
	WORD y;
} fd32_mouse_getbutton_t;

typedef struct fd32_mouse_subroutine
{
	WORD call_mask;
	WORD cs;
	WORD ip;
} fd32_mouse_subroutine_t;

/* text mode video memory */
typedef WORD (*text_screen_t)[][80];
static text_screen_t screen = (text_screen_t)0xB8000;
/* mouse status */
static volatile int screen_w = 640, screen_h = 400;
static volatile int text_x = 0, text_y = 0, x, y, text_cell_w = 8, text_cell_h = 16;
static volatile WORD buttons;
/* text mode cursor and mask */
static WORD save = 0, scrmask = 0x77ff, curmask = 0x7700;
static int cursor_visible = 0;
/* interrupt subroutine */
static fd32_mouse_subroutine_t subroutine;
#define CALLMASK_MOUSE_MOVE			0x01
#define CALLMASK_LBUTTON_PRESSED	0x02
#define CALLMASK_LBUTTON_RELEASED	0x04
#define CALLMASK_RBUTTON_PRESSED	0x08
#define CALLMASK_RBUTTON_RELEASED	0x10
#define CALLMASK_MBUTTON_PRESSED	0x20
#define CALLMASK_MBUTTON_RELEASED	0x40

void int33_set_screen(int w, int h)
{
	screen_w = w;
	screen_h = h;
}

void int33_set_text_cell(int w, int h)
{
	text_cell_w = w;
	text_cell_h = h;
}

static void pos(int posx, int posy)
{
	x = posx;
	y = posy;

	/* Get the text mode new X and new Y */
	text_x = x/text_cell_w;
	text_y = y/text_cell_h;
	/* Save the previous character */
	save = (*screen)[text_y][text_x];
	/* Display the cursor */
	if (cursor_visible) {
		(*screen)[text_y][text_x] &= scrmask;
		(*screen)[text_y][text_x] ^= curmask;
	}
}

static void cb(const fd32_mousedata_t *data)
{
	static WORD pre_buttons = 0;
	buttons = data->buttons;
	x += data->axes[MOUSE_X];
	y -= data->axes[MOUSE_Y];

	if (x < 0 || x >= screen_w || y < 0 || y >= screen_h) {
		/* Recover the original coordinates */
		x -= data->axes[MOUSE_X];
		y += data->axes[MOUSE_Y];
	} else {
		if(save != 0)
			(*screen)[text_y][text_x] = save;
		pos(x, y);
	}

	if (subroutine.call_mask) {
		int call_mask = 0;
		if (data->axes[MOUSE_X] || data->axes[MOUSE_Y])
			call_mask |= CALLMASK_MOUSE_MOVE;
		if (buttons&MOUSE_LBUT)
			call_mask |= CALLMASK_LBUTTON_PRESSED;
		else if (pre_buttons&MOUSE_LBUT)
			call_mask |= CALLMASK_LBUTTON_RELEASED;
		if (buttons&MOUSE_RBUT)
			call_mask |= CALLMASK_RBUTTON_PRESSED;
		else if (pre_buttons&MOUSE_RBUT)
			call_mask |= CALLMASK_RBUTTON_RELEASED;
		if (buttons&MOUSE_MBUT)
			call_mask |= CALLMASK_MBUTTON_PRESSED;
		else if (pre_buttons&MOUSE_MBUT)
			call_mask |= CALLMASK_MBUTTON_RELEASED;
		pre_buttons = buttons;

		if (subroutine.call_mask&call_mask)
		{
			X_REGS16 in, out;
			X_SREGS16 s;
			s.cs = subroutine.cs;
			s.ds = 0;
			s.es = 0;
			s.ss = 0;
			in.x.ax = call_mask;
			in.x.bx = buttons&(MOUSE_LBUT|MOUSE_RBUT|MOUSE_MBUT);
			in.x.cx = text_x*8;
			in.x.dx = text_y*8;
			in.x.si = 0;
			in.x.di = 0;

			vm86_callRMPROC (subroutine.ip, &in, &out, &s);
		}
	}
}

int _mousebios_init(void)
{
	fd32_request_t *request;
	int res;

	if ((res = fd32_dev_search("mouse")) < 0)
	{
		LOG_PRINTF("no mouse driver\n");
		return -1;
	}

	if (fd32_dev_get(res, &request, NULL, NULL, 0) < 0)
	{
		LOG_PRINTF("no mouse request calls\n");
		return -1;
	}
	res = request(FD32_MOUSE_SETCB, cb);

	return res;
}


int mousebios_int(union rmregs *r)
{
	int res = 0;

	switch(r->x.ax)
	{
		/* MS MOUSE - RESET DRIVER AND READ STATUS */
		case 0x0000:
#ifdef __INT33_DEBUG__
			LOG_PRINTF("Reset mouse driver\n");
#endif
			r->x.ax = 0xffff;
			r->x.bx = 0x0002;
			break;
		/* MS MOUSE v1.0+ - SHOW MOUSE CURSOR */
		case 0x0001:
#ifdef __INT33_DEBUG__
			LOG_PRINTF("Show Mouse cursor\n");
#endif
			cursor_visible = 1;
			save = (*screen)[text_y][text_x];
			(*screen)[text_y][text_x] &= scrmask;
			(*screen)[text_y][text_x] ^= curmask;
			break;
		/* MS MOUSE v1.0+ - HIDE MOUSE CURSOR */
		case 0x0002:
#ifdef __INT33_DEBUG__
			LOG_PRINTF("Hide mouse cursor\n");
#endif
			cursor_visible = 0;
			(*screen)[text_y][text_x] = save;
			break;
		/* MS MOUSE v1.0+ - RETURN POSITION AND BUTTON STATUS */
		case 0x0003:
			r->x.bx = buttons&0x07;
			r->x.cx = text_x*8;
			r->x.dx = text_y*8;
#ifdef __INT33_DEBUG__
			if(r->x.bx != 0)
				LOG_PRINTF("[MOUSE BIOS] Button clicked, cx: %x\tdx: %x\tbx: %x\n", r->x.cx, r->x.dx, r->x.bx);
#endif
			break;
		/* MS MOUSE v1.0+ - POSITION MOUSE CURSOR */
		case 0x0004:
			pos(r->x.cx, r->x.dx);
			break;
		/* MS MOUSE v1.0+ - RETURN BUTTON PRESS DATA */
		case 0x0005:
			r->x.ax = buttons; /* button states */
			r->x.bx = 0; 
			r->x.cx = text_x;
			r->x.dx = text_x;
			break;
		/* MS MOUSE v1.0+ - RETURN BUTTON RELEASE DATA */
		case 0x0006:
			r->x.ax = buttons; /* button states */
			r->x.bx = 0; 
			r->x.cx = text_x;
			r->x.dx = text_x;
			break;
		/* MS MOUSE v1.0+ - DEFINE HORIZONTAL CURSOR RANGE */
		case 0x0007:
			/* r->x.cx = 0; minimum column */
			/* r->x.dx = 639; maximum column */
			res = 0; /* TODO: malfunction */
			break;
		/* MS MOUSE v1.0+ - DEFINE VERTICAL CURSOR RANGE */
		case 0x0008:
			/* r->x.cx = 0; minimum row */
			/* r->x.dx = 479; maximum row */
			res = 0; /* TODO: malfunction */
			break;
		/* MS MOUSE v3.0+ - DEFINE TEXT CURSOR */
		case 0x000A:
			if (r->x.bx == 0) {
				scrmask = r->x.cx;
				curmask = r->x.dx;
			} else {
				scrmask = 0xff;
				curmask = 0xff;
			}
			break;
		/* MS MOUSE v1.0+ - DEFINE INTERRUPT SUBROUTINE PARAMETERS */
		case 0x000C:
			LOG_PRINTF("DEFINE INTERRUPT SUBROUTINE PARAMETERS: %x\n", r->x.cx);
			subroutine.cs = r->x.es;
			subroutine.ip = r->x.dx;
			subroutine.call_mask = r->x.cx;
			break;
		/* Genius Mouse 9.06 - GET NUMBER OF BUTTONS */
		case 0x0011:
			r->x.ax = 0xFFFF;
			r->x.bx = 2; /* Number of buttons */
			break;
		/* MS MOUSE v6.0+ - RETURN DRIVER STORAGE REQUIREMENTS */
		case 0x0015:
#ifdef __INT33_DEBUG__
			LOG_PRINTF("RETURN DRIVER STORAGE REQUIREMENTS NOT USED, SIZE SET TO ZERO\n");
#endif
			r->x.bx = 0;
			break;
		/* MS MOUSE v6.0+ - SOFTWARE RESET */
		case 0x0021:
#ifdef __INT33_DEBUG__
			LOG_PRINTF("Software reset mouse driver\n");
#endif
			r->x.ax = 0xffff;
			r->x.bx = 0x0002;
			break;
		default:
			message("[MOUSE BIOS] Unimplemeted INT 33H AL=0x%x!!!\n", r->h.al);
			res = 0;
			break;
	}
	
	return res;
}
