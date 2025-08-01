/**
 * vga.c
 */

#include "keycodes.h"
#include "stdbool.h"
#include <kernel.h>
#include <sys/types.h>
#include <sys/io.h>
#include <string.h>
#include <vga.h>

u16	VGA_tmp[VGA_WIDTH * VGA_HEIGHT] = {0};
u16	VGA0_screen[VGA_WIDTH * VGA_HEIGHT] = {0};
u16	VGA1_screen[VGA_WIDTH * VGA_HEIGHT] = {0};

VGA_ctx		VGA_CTX = {0};
VGA_screen	VGA0 = { .screen = (u16 *)VGA0_screen };
VGA_screen	VGA1 = { .screen = (u16 *)VGA1_screen };

__inline void
vga_cursor_update(void)
{
	u16 pos = (VGA_CTX.row * VGA_WIDTH) + VGA_CTX.col;

	outb(VGA_INDEX_BYTE, 0x0F);
	outb(VGA_DATA_BYTE,  pos & 0xFF);
	outb(VGA_INDEX_BYTE, 0x0E);
	outb(VGA_DATA_BYTE,  pos >> 8);
}

static void
vga_status_update(u16 *vga)
{
    u8  attr = VGA_COLOR_WHITE | VGA_COLOR_RED << 4;
    u8  i = 0;
    u32 offset = VGA_WIDTH * (VGA_HEIGHT - 1);

    for (; i < 9; ++i)
        vga[offset + i] = "@ screen "[i] | attr << 8;
    vga[offset + i++] = (VGA_CTX.n + 48) | attr << 8;
    for (; i < VGA_WIDTH - 7; ++i)
        vga[offset + i] = ' ' | attr << 8;
    vga[offset + i++] = (VGA_CTX.row / 10 + 48) | attr << 8;
    vga[offset + i++] = (VGA_CTX.row % 10 + 48) | attr << 8;
    vga[offset + i++] = ',' | attr << 8;
    vga[offset + i++] = ' ' | attr << 8;
    vga[offset + i++] = (VGA_CTX.col / 10 + 48) | attr << 8;
    vga[offset + i++] = (VGA_CTX.col % 10 + 48) | attr << 8;
    vga[offset + i++] = ' ' | attr << 8;
}

static void
vga_update(void)
{
    vga_status_update((u16 *)VGA_SCREEN);
    vga_cursor_update();
}

__inline void
vga_scroll(void)
{
	u32	offset = VGA_WIDTH * (VGA_HEIGHT - 1 - VGA_STATUS_SIZE);

	memcpy((u16 *)VGA_SCREEN, (u16 *)VGA_SCREEN + VGA_WIDTH, offset * 2);

	for (u32 i = 0; i < VGA_WIDTH; ++i)
		((u16 *)VGA_SCREEN + offset)[i] = (VGA_CURSOR_ATTR << 8);
}

__inline void
vga_newline(void)
{
	VGA_CTX.col = 0;
	if (VGA_CTX.row + 1 == VGA_HEIGHT - VGA_STATUS_SIZE)
		vga_scroll();
	else
		VGA_CTX.row++;
	((u16 *)VGA_SCREEN)[VGA_CTX.row * VGA_WIDTH + VGA_CTX.col] = (VGA_CURSOR_ATTR << 8) | ' ';
	vga_update();
}

#define	TAB_SIZE	4

__inline void
vga_tabulation(void)
{
	if (VGA_CTX.col > VGA_WIDTH - 4 - 1)
	{
		vga_newline();
		return ;
	}
	u8 spacing = TAB_SIZE - (VGA_CTX.col & (TAB_SIZE - 1));
	for (u8 i = 0; i < spacing; vga_putc(' '), ++i);
}

__inline u32
vga_go_back(void)
{
	if (!VGA_CTX.col)
	{
		if (!VGA_CTX.row)
			return (1);
		VGA_CTX.row--;
		VGA_CTX.col = VGA_WIDTH;
	}
	VGA_CTX.col--;
	return (0);
}

__inline void
vga_backspace(void)
{
	while (!vga_go_back())
	{
		u16	*current = &((u16 *)VGA_SCREEN)[VGA_CTX.row * VGA_WIDTH + VGA_CTX.col];

		if ((*current & 0xFF) != 0)
		{
			*current = VGA_CURSOR_ATTR << 8;
			break ;
		}
	}
	vga_update();
}

__inline void
vga_arrow(u8 arrow)
{
    if (arrow == KEY_LEFT)
        VGA_CTX.col -= !!(VGA_CTX.col);
    if (arrow == KEY_RIGHT)
        VGA_CTX.col += (VGA_CTX.col < VGA_WIDTH - 1);
    if (arrow == KEY_UP)
        VGA_CTX.row -= !!(VGA_CTX.row);
    if (arrow == KEY_DOWN)
        VGA_CTX.row += (VGA_CTX.row < VGA_HEIGHT - 1 - VGA_STATUS_SIZE);
    vga_update();
}

void
vga_screen_clear(u16 *vga)
{
	for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i)
		vga[i] = (VGA_CURSOR_ATTR << 8);
}

void
vga_init(void)
{
	VGA_CTX.attr = VGA_CURSOR_ATTR;
	VGA0.ctx.attr = VGA_CURSOR_ATTR;
	VGA1.ctx.attr = VGA_CURSOR_ATTR;
	
	vga_screen_clear((u16 *)VGA_SCREEN);
	vga_screen_clear(VGA0.screen);
	vga_screen_clear(VGA1.screen);

    vga_status_update((u16 *)VGA_SCREEN);
    VGA_CTX.n = 1;
    vga_status_update((u16 *)VGA0.screen);
    VGA_CTX.n = 2;
    vga_status_update((u16 *)VGA1.screen);
    VGA_CTX.n = 0;

	outb(VGA_INDEX_BYTE, 0x0A);
    outb(VGA_DATA_BYTE, 0x00);
    outb(VGA_INDEX_BYTE, 0x0B);
    vga_cursor_set(0, 0);
}

void
vga_bg_set(u8 bg)
{
	VGA_CTX.attr &= 0xF;
	VGA_CTX.attr |= bg << 4;
}

void
vga_fg_set(u8 fg)
{
	VGA_CTX.attr &= 0xF0;
	VGA_CTX.attr |= fg;
}

void
vga_attr_set(u8 fg, u8 bg)
{
	VGA_CTX.attr = bg << 4 | fg;
}

void
vga_cursor_set(u8 x, u8 y)
{
	VGA_CTX.col = x;
	VGA_CTX.row = y;
	vga_cursor_update();
}

void
vga_screen_shift(void)
{
	static bool	n = true;
    static int  ns = 1;
	VGA_ctx		tmp;
	VGA_ctx		*ctx = n ? &VGA0.ctx : &VGA1.ctx;
	void		*screen = n ? VGA0_screen : VGA1_screen;

	tmp = VGA_CTX;
	VGA_CTX = *ctx;
	*ctx = tmp;

    VGA_CTX.n = ns;
    ns = (ns + 1) % 3;

	
	memcpy(VGA_tmp, (u16 *)VGA_SCREEN, VGA_WIDTH * VGA_HEIGHT * 2);
	for(u32 i = 0; i < VGA_HEIGHT;i++)
		((u16 *)VGA_SCREEN)[(i * VGA_WIDTH) + (VGA_WIDTH - 1)] = 0x04 << 8 | 0xDB;
	for (u32 colon = 0; colon < VGA_WIDTH; ++colon)
	{
		memcpy((u16 *)VGA_SCREEN, (u16 *)VGA_SCREEN + 1, (VGA_WIDTH * VGA_HEIGHT - 1) * 2);
		for (u32 i = 0; i < VGA_HEIGHT; ++i)
		{
			u32 line = i * VGA_WIDTH;
			((u16 *)VGA_SCREEN)[line + (VGA_WIDTH - 1)] = ((u16 *)screen)[line + colon];
		}
         nop_loop(0xffffffff);
         nop_loop(0xffffffff);
         nop_loop(0xffffffff);
	}
	memcpy(screen, VGA_tmp, VGA_HEIGHT * VGA_WIDTH * 2);
	vga_update();

	n = !n;
}

void
vga_putc(char c)
{
	switch ((u8)c)
	{
		case '\n':
			vga_newline();
			return ;
		case '\t':
			vga_tabulation();
			return ;
		case '\b':
			vga_backspace();
			return ;
        case KEY_DOWN:
        case KEY_UP:
        case KEY_LEFT:
        case KEY_RIGHT:
            vga_arrow(c);
            return ;
		default:
			break ;
	}
	((u16 *)VGA_SCREEN)[VGA_CTX.row * VGA_WIDTH + VGA_CTX.col] = (VGA_CTX.attr << 8) | c;
	VGA_CTX.col++;
    if (VGA_CTX.col == VGA_WIDTH)
        vga_newline();
    vga_update();
}

void 
vga_puts(const char *str)
{
	while(*str)
		vga_putc(*(str++));
}

void 
vga_write(const char *str, u32 len)
{
	u32 i;

	i = 0;
	while(i < len)
		vga_putc(str[i++]);
}
