#include "pokerom.h"

static u8 *buffer;		// output buffer
static u16 a188 = 0x0000;	// focus part1
static u16 a310 = 0x0188;	// focus part2

static u8 tile_x;		// d0a1
static u8 tile_y;		// d0a2
static u8 sprite_width;		// d0a3
static u8 sprite_height;	// d0a4
static u8 current_byte;		// d0a5
static u8 current_bit;		// d0a6
static u8 p_flag;		// d0a7
static u8 buffer_flag;		// d0a8
static u8 misc_flag;		// d0a9
static u8 input_flag;		// d0aa
static int current_addr;	// d0ab-d0ac
static u16 p1;			// d0ad-d0ae
static u16 p2;			// d0af-d0b0
static u16 input_p1;		// d0b1-d0b2
static u16 input_p2;		// d0b3-d0b4

static void write_buffer(u16 addr, u8 v)
{
	if (addr < 0x188 * 2)
		buffer[addr] = v;
}

static u8 read_buffer(u16 addr)
{
	return (addr < 0x188 * 2) ? buffer[addr] : 0;
}

static u8 sprite_get_next_byte() // 268B
{
	return gl_stream[current_addr++];
}

static u8 sprite_get_next_bit() // 2670
{
	if (--current_bit == 0) {
		current_byte = sprite_get_next_byte();
		current_bit = 8;
	}
	current_byte = ((current_byte << 1) | ((current_byte & (1 << 7)) >> 7)) & 0xff;
	return current_byte & 1;
}

static u16 sprite_set_p1_p2(u16 hl) // 2897
{
	p1 = p2 = hl;
	return hl;
}

static void sprite_update_p1(u8 a) // 2649
{
	switch (p_flag) {
	case 1:
		a = (a << 2) & 0xff;
		break;
	case 2:
		a = swap_u8(a);
		break;
	case 3:
		a = (((a & 1) << 7) | (a >> 1)) & 0xff;
		a = (((a & 1) << 7) | (a >> 1)) & 0xff;
		break;
	}
	write_buffer(p1, read_buffer(p1) | a);
}

static void sprite_reset_p1_p2() // 2841
{
	if (buffer_flag & 1) {
		p2 = a310;
		p1 = a188;
	} else {
		p2 = a188;
		p1 = a310;
	}
}

static u8 sprite_update_input_ptr(u8 nibble, u16 *_hl, u16 *_de) // 276D
{
	u16 hl = *_hl, de = *_de;
	int condition, swap_flag;
	u8 a;

	swap_flag = nibble & 1;
	nibble >>= 1;

	hl = (hl & 0xff00) | nibble;
	condition = (input_flag) ? (de & (1 << 3)) : (de & (1 << 0));
	de = (de & 0xff00) | (hl & 0x00ff);
	hl = (condition == 0) ? input_p1 : input_p2;
	hl += (de & 0x00ff);

	a = gl_stream[hl];
	if (swap_flag == 0)
		a = swap_u8(a);

	a = a & 0x0f;
	de = (de & 0xff00) | a;

	*_hl = hl;
	*_de = de;
	return a;
}

static void sprite_load_data(u16 p) // 26D4
{
	u16 hl, de;
	u8 z, nibble;

	tile_x = 0;
	tile_y = 0;
	sprite_set_p1_p2(p);
	if (input_flag) {
		hl = input_p1 = 0x27b7;
		de = input_p2 = 0x27bf;
	} else {
		hl = input_p1 = 0x27a7;
		de = input_p2 = 0x27af;
	}

	de = de & 0xff00;

	// 2704
	while (1) {
		hl = p1;
		z = read_buffer(hl);

		nibble = sprite_update_input_ptr(high_nibble(z), &hl, &de);
		nibble = swap_u8(nibble);
		de = (nibble << 8) | (de & 0x00ff);

		nibble = sprite_update_input_ptr(low_nibble(z), &hl, &de);
		nibble = (de >> 8) | nibble;

		hl = p1;

		// 2729
		write_buffer(p1, nibble);

		hl += sprite_height;

		// 2731
		p1 = hl;
		tile_x += 8;
		if (tile_x != sprite_width)
			continue;

		// 2747
		de = de & 0xff00; // e = 0
		tile_x = 0;
		tile_y++;
		if (tile_y == sprite_height) {
			tile_y = 0;
			return;
		}

		sprite_set_p1_p2(p2 + 1);
	}
}

static void sprite_uncompress_data() // 27C7
{
	u16 hl, de;

	tile_x = tile_y = 0;
	sprite_reset_p1_p2();
	sprite_load_data(p1);
	sprite_reset_p1_p2();

	// 27DF
	hl = p1;
	de = p2;

	// 27EF
	while (1) {
		if (input_flag) {
			// 27F6
			u8 a, c, v;

			v = read_buffer(de);
			a = gl_stream[0x2867 + high_nibble(v)];
			c = swap_u8(a);
			a = gl_stream[0x2867 + low_nibble(v)];
			a |= c;
			write_buffer(de, a);
		}

		// 280b
		write_buffer(de, read_buffer(de) ^ read_buffer(hl));
		hl++;

		de++;
		tile_y++;
		if (tile_y != sprite_height)
			continue;
		tile_y = 0;

		tile_x += 8;
		if (tile_x != sprite_width)
			continue;
		tile_x = 0;
		return;
	}
}

enum {Z_RET, Z_END, Z_CONTINUE};

#define HANDLE_RET(r) do {\
	switch (r) {\
	case Z_RET: break;		/* normal function call */ \
	case Z_END: return;		/* stack pwned, return is a return in calling function */ \
	case Z_CONTINUE: goto start;	/* stack pwned, but continue loop in calling function */ \
	}\
} while (0)

static int f25d8()
{
	if ((tile_y + 1) != sprite_height) {
		tile_y++;
		p1++;
		return Z_RET;
	}

	// 25F6
	tile_y = 0;
	if (p_flag) {
		p_flag--;
		p1 = p2;
		return Z_RET;
	}

	// 2610
	p_flag = 3;
	tile_x += 8;
	if (tile_x != sprite_width) {
		sprite_set_p1_p2(p1 + 1);
		return Z_RET;
	}

	/* Break context here: calling function must end after processing */

	// 2630
	tile_x = 0;
	if (!(buffer_flag & (1 << 1))) {
		buffer_flag = (buffer_flag ^ 1) | (1 << 1);
		return Z_CONTINUE; // ♥ How to goto over function ♥
	}

	// 2646 (-> 26BF)
	if (misc_flag == 2) {
		// 2877
		u8 input_flag_backup = input_flag;

		sprite_reset_p1_p2();
		input_flag = 0;
		sprite_load_data(p2);
		sprite_reset_p1_p2();
		input_flag = input_flag_backup;
		sprite_uncompress_data(); // jp 27c7
		return Z_END;
	}

	// 26C7
	if (misc_flag) {
		sprite_uncompress_data(); // jp 27c7
		return Z_END;
	}

	// 26CB
	sprite_load_data(a188);
	sprite_load_data(a310);
	return Z_END;
}

void uncompress_sprite(u8 *dest, int addr) // 251A
{
	u8 byte, b, b1, b2, c;
	u16 p, de;
	int r;

	current_addr = addr;
	buffer = dest;

	// Init (251D)
	memset(buffer, 0, 0x310);
	current_bit = 1;
	p_flag = 3;

	tile_x = 0;
	tile_y = 0;
	buffer_flag = 0;

	byte = sprite_get_next_byte();
	sprite_height = low_nibble(byte) * 8;
	sprite_width = high_nibble(byte) * 8;
	buffer_flag = sprite_get_next_bit();

start:
	// 2556
	sprite_set_p1_p2(((buffer_flag & 1) == 0) ? a188 : a310);
	if (buffer_flag & (1 << 1)) { /* 0b10 or 0b11 */
		b = sprite_get_next_bit();
		if (b) {
			b = sprite_get_next_bit();
			b++;
		}
		misc_flag = b;
	}

	// 257A
	if (!sprite_get_next_bit())
		goto lbl_2595;

lbl_2580:
	b1 = sprite_get_next_bit();
	b2 = sprite_get_next_bit();

	b2 = b2 | (b1 << 1);
	if (b2) {
		sprite_update_p1(b2);
		r = f25d8();
		HANDLE_RET(r);
		goto lbl_2580;
	}

lbl_2595:
	c = 0;
	while ((b = sprite_get_next_bit()) != 0)
		c++;

	p = 0x269f + 2 * c;
	p = *(u16*)&gl_stream[p];

	c++;
	de = 0x0000;

	while (1) {
		de |= sprite_get_next_bit();
		if (--c == 0)
			break;
		de <<= 1;
	}

	de += p;

	do {
		sprite_update_p1(0);
		r = f25d8();
		HANDLE_RET(r);
		de--;
	} while (de);
	goto lbl_2580;
}
