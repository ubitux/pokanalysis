/*
 *  This file is part of Pokanalysis.
 *
 *  Pokanalysis
 *  Copyright © 2010-2011 Clément Bœsch
 *
 *  Pokanalysis is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Pokanalysis is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Pokanalysis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pokerom.h"

struct instruction {
	char *label;
	int type;
};

enum {P_NONE = 0, P_UCHAR8, P_CHAR8, P_WORD};

static struct instruction default_ins_set[] = {
	[0x00] = {.label="nop"},
	[0x01] = {.label="ld bc,%04x", P_WORD},
	[0x02] = {.label="ld (bc),a"},
	[0x03] = {.label="inc bc"},
	[0x04] = {.label="inc b"},
	[0x05] = {.label="dec b"},
	[0x06] = {.label="ld b,%02x", P_UCHAR8},
	[0x07] = {.label="rlca"},
	[0x08] = {.label="ld (%04x),sp", P_WORD},
	[0x09] = {.label="add hl,bc"},
	[0x0a] = {.label="ld a,(bc)"},
	[0x0b] = {.label="dec bc"},
	[0x0c] = {.label="inc c"},
	[0x0d] = {.label="dec c"},
	[0x0e] = {.label="ld c,%02x", P_UCHAR8},
	[0x0f] = {.label="rrca"},

	[0x10] = {.label="stop"}, /* 0x10 0x00 */
	[0x11] = {.label="ld de,%04x", P_WORD},
	[0x12] = {.label="ld (de),a"},
	[0x1a] = {.label="ld a,(de)"},
	[0x1b] = {.label="dec de"},
	[0x13] = {.label="inc de"},
	[0x14] = {.label="inc d"},
	[0x15] = {.label="dec d"},
	[0x16] = {.label="ld d,%02x", P_UCHAR8},
	[0x17] = {.label="rla"},
	[0x18] = {.label="jr %04x", P_CHAR8},
	[0x19] = {.label="add hl,de"},
	[0x1c] = {.label="inc e"},
	[0x1d] = {.label="dec e"},
	[0x1e] = {.label="ld e,%02x", P_UCHAR8},
	[0x1f] = {.label="rra"},

	[0x20] = {.label="jr nz,%04x", P_CHAR8},
	[0x21] = {.label="ld hl,%04x", P_WORD},
	[0x22] = {.label="ldi (hl),a"},
	[0x23] = {.label="inc hl"},
	[0x24] = {.label="inc h"},
	[0x25] = {.label="dec h"},
	[0x27] = {.label="daa"},
	[0x28] = {.label="jr z,%04x", P_CHAR8},
	[0x2a] = {.label="ldi a,(hl)"},
	[0x2b] = {.label="dec hl"},
	[0x26] = {.label="ld h,%02x", P_UCHAR8},
	[0x29] = {.label="add hl,hl"},
	[0x2c] = {.label="inc l"},
	[0x2d] = {.label="dec l"},
	[0x2e] = {.label="ld l,%02x", P_UCHAR8},
	[0x2f] = {.label="cpl"},

	[0x30] = {.label="jr nc,%04x", P_CHAR8},
	[0x31] = {.label="ld sp,%04x", P_WORD},
	[0x32] = {.label="ldd (hl),a"},
	[0x33] = {.label="inc sp"},
	[0x34] = {.label="inc (hl)"},
	[0x35] = {.label="dec (hl)"},
	[0x37] = {.label="scf"},
	[0x38] = {.label="jr c,%04x", P_CHAR8},
	[0x3a] = {.label="ldd a,(hl)"},
	[0x3b] = {.label="dec sp"},
	[0x3c] = {.label="inc a"},
	[0x3d] = {.label="dec a"},
	[0x36] = {.label="ld (hl),%02x", P_UCHAR8},
	[0x39] = {.label="add hl,sp"},
	[0x3e] = {.label="ld a,%02x", P_UCHAR8},
	[0x3f] = {.label="ccf"},

	[0x40] = {.label="ld b,b"},
	[0x41] = {.label="ld b,c"},
	[0x42] = {.label="ld b,d"},
	[0x43] = {.label="ld b,e"},
	[0x44] = {.label="ld b,h"},
	[0x45] = {.label="ld b,l"},
	[0x46] = {.label="ld b,(hl)"},
	[0x47] = {.label="ld b,a"},

	[0x48] = {.label="ld c,b"},
	[0x49] = {.label="ld c,c"},
	[0x4a] = {.label="ld c,d"},
	[0x4b] = {.label="ld c,e"},
	[0x4c] = {.label="ld c,h"},
	[0x4d] = {.label="ld c,l"},
	[0x4e] = {.label="ld c,(hl)"},
	[0x4f] = {.label="ld c,a"},

	[0x50] = {.label="ld d,b"},
	[0x51] = {.label="ld d,c"},
	[0x52] = {.label="ld d,d"},
	[0x53] = {.label="ld d,e"},
	[0x54] = {.label="ld d,h"},
	[0x55] = {.label="ld d,l"},
	[0x56] = {.label="ld d,(hl)"},
	[0x57] = {.label="ld d,a"},

	[0x58] = {.label="ld e,b"},
	[0x59] = {.label="ld e,c"},
	[0x5a] = {.label="ld e,d"},
	[0x5b] = {.label="ld e,e"},
	[0x5c] = {.label="ld e,h"},
	[0x5d] = {.label="ld e,l"},
	[0x5e] = {.label="ld e,(hl)"},
	[0x5f] = {.label="ld e,a"},

	[0x60] = {.label="ld h,b"},
	[0x61] = {.label="ld h,c"},
	[0x62] = {.label="ld h,d"},
	[0x63] = {.label="ld h,e"},
	[0x64] = {.label="ld h,h"},
	[0x65] = {.label="ld h,l"},
	[0x66] = {.label="ld h,(hl)"},
	[0x67] = {.label="ld h,a"},

	[0x68] = {.label="ld l,b"},
	[0x69] = {.label="ld l,c"},
	[0x6a] = {.label="ld l,d"},
	[0x6b] = {.label="ld l,e"},
	[0x6c] = {.label="ld l,h"},
	[0x6d] = {.label="ld l,l"},
	[0x6e] = {.label="ld l,(hl)"},
	[0x6f] = {.label="ld l,a"},

	[0x70] = {.label="ld (hl),b"},
	[0x71] = {.label="ld (hl),c"},
	[0x72] = {.label="ld (hl),d"},
	[0x73] = {.label="ld (hl),e"},
	[0x74] = {.label="ld (hl),h"},
	[0x75] = {.label="ld (hl),l"},
	[0x76] = {.label="halt"},
	[0x77] = {.label="ld (hl),a"},

	[0x78] = {.label="ld a,b"},
	[0x79] = {.label="ld a,c"},
	[0x7a] = {.label="ld a,d"},
	[0x7b] = {.label="ld a,e"},
	[0x7c] = {.label="ld a,h"},
	[0x7d] = {.label="ld a,l"},
	[0x7e] = {.label="ld a,(hl)"},
	[0x7f] = {.label="ld a,a"},

	[0x80] = {.label="add b"},
	[0x81] = {.label="add c"},
	[0x82] = {.label="add d"},
	[0x83] = {.label="add e"},
	[0x84] = {.label="add h"},
	[0x85] = {.label="add l"},
	[0x86] = {.label="add (hl)"},
	[0x87] = {.label="add a"},
	[0x88] = {.label="adc b"},
	[0x89] = {.label="adc c"},
	[0x8a] = {.label="adc d"},
	[0x8b] = {.label="adc e"},
	[0x8c] = {.label="adc h"},
	[0x8d] = {.label="adc l"},
	[0x8e] = {.label="adc (hl)"},
	[0x8f] = {.label="adc a"},

	[0x90] = {.label="sub b"},
	[0x91] = {.label="sub c"},
	[0x92] = {.label="sub d"},
	[0x93] = {.label="sub e"},
	[0x94] = {.label="sub h"},
	[0x95] = {.label="sub l"},
	[0x96] = {.label="sub (hl)"},
	[0x97] = {.label="sub a"},
	[0x98] = {.label="sbc b"},
	[0x99] = {.label="sbc c"},
	[0x9a] = {.label="sbc d"},
	[0x9b] = {.label="sbc e"},
	[0x9c] = {.label="sbc h"},
	[0x9d] = {.label="sbc l"},
	[0x9e] = {.label="sbc (hl)"},
	[0x9f] = {.label="sbc a"},

	[0xa0] = {.label="and b"},
	[0xa1] = {.label="and c"},
	[0xa2] = {.label="and d"},
	[0xa3] = {.label="and e"},
	[0xa4] = {.label="and h"},
	[0xa5] = {.label="and l"},
	[0xa6] = {.label="and (hl)"},
	[0xa7] = {.label="and a"},
	[0xa8] = {.label="xor b"},
	[0xa9] = {.label="xor c"},
	[0xaa] = {.label="xor d"},
	[0xab] = {.label="xor e"},
	[0xac] = {.label="xor h"},
	[0xad] = {.label="xor l"},
	[0xae] = {.label="xor (hl)"},
	[0xaf] = {.label="xor a"},

	[0xb0] = {.label="or b"},
	[0xb1] = {.label="or c"},
	[0xb2] = {.label="or d"},
	[0xb3] = {.label="or e"},
	[0xb4] = {.label="or h"},
	[0xb5] = {.label="or l"},
	[0xb6] = {.label="or (hl)"},
	[0xb7] = {.label="or a"},
	[0xb8] = {.label="cp b"},
	[0xb9] = {.label="cp c"},
	[0xba] = {.label="cp d"},
	[0xbb] = {.label="cp e"},
	[0xbc] = {.label="cp h"},
	[0xbd] = {.label="cp l"},
	[0xbe] = {.label="cp (hl)"},
	[0xbf] = {.label="cp a"},

	[0xc0] = {.label="ret nz"},
	[0xc1] = {.label="pop bc"},
	[0xc2] = {.label="jp nz %04x", P_WORD},
	[0xc3] = {.label="jp %04x", P_WORD},
	[0xc4] = {.label="call nz %04x", P_WORD},
	[0xc5] = {.label="push bc"},
	[0xc6] = {.label="add a,%02x", P_UCHAR8},
	[0xc7] = {.label="rst 00"},
	[0xc8] = {.label="ret z"},
	[0xc9] = {.label="ret"},
	[0xca] = {.label="jp z,%04x", P_WORD},

	/* 0xCB: E.label=xtended opcode */

	[0xcc] = {.label="call z,%04x", P_WORD},
	[0xcd] = {.label="call %04x", P_WORD},
	[0xce] = {.label="adc a,%02x", P_UCHAR8},
	[0xcf] = {.label="rst 08"},

	[0xd0] = {.label="ret nc"},
	[0xd1] = {.label="pop de"},
	[0xd2] = {.label="jp nc %04x", P_WORD},

	[0xd4] = {.label="call nc %04x", P_WORD},
	[0xd5] = {.label="push de"},
	[0xd6] = {.label="sub a,%02x", P_UCHAR8},
	[0xd7] = {.label="rst 10"},
	[0xd8] = {.label="ret c"},
	[0xd9] = {.label="reti"},
	[0xda] = {.label="jp c,%04x", P_WORD},

	[0xdc] = {.label="call c,%04x", P_WORD},

	[0xde] = {.label="sbc a,%02x", P_UCHAR8},
	[0xdf] = {.label="rst 18"},

	[0xe0] = {.label="ld (ff00+%02x),a", P_UCHAR8},
	[0xe1] = {.label="pop hl"},
	[0xe2] = {.label="ld (ff00+c),a"},

	[0xe5] = {.label="push hl"},
	[0xe6] = {.label="and a,%02x", P_UCHAR8},
	[0xe7] = {.label="rst 20"},
	[0xe8] = {.label="add sp,%02x", P_UCHAR8},
	[0xe9] = {.label="jp hl"},
	[0xea] = {.label="ld (%04x),a", P_WORD},

	[0xee] = {.label="xor a,%02x", P_UCHAR8},
	[0xef] = {.label="rst 28"},

	[0xf0] = {.label="ld a,(ff00+%02x)", P_UCHAR8},
	[0xf1] = {.label="pop af"},
	[0xf2] = {.label="ld a,(ff00+c)"},
	[0xf3] = {.label="di"},

	[0xf5] = {.label="push af"},
	[0xf6] = {.label="or a,%02x", P_UCHAR8},
	[0xf7] = {.label="rst 30"},
	[0xf8] = {.label="ld hl,sp+%02x", P_UCHAR8},
	[0xf9] = {.label="ld sp,hl"},
	[0xfa] = {.label="ld a,(%04x)", P_WORD},
	[0xfb] = {.label="ei"},

	[0xfe] = {.label="cp a,%02x", P_UCHAR8},
	[0xff] = {.label="rst 38"},
};

static struct instruction extended_ins_set[] = {
	[0x00] = {.label="rlc b"},
	[0x01] = {.label="rlc c"},
	[0x02] = {.label="rlc d"},
	[0x03] = {.label="rlc e"},
	[0x04] = {.label="rlc h"},
	[0x05] = {.label="rlc l"},
	[0x06] = {.label="rlc (hl)"},
	[0x07] = {.label="rlc a"},
	[0x08] = {.label="rrc b"},
	[0x09] = {.label="rrc c"},
	[0x0a] = {.label="rrc d"},
	[0x0b] = {.label="rrc e"},
	[0x0c] = {.label="rrc h"},
	[0x0d] = {.label="rrc l"},
	[0x0e] = {.label="rrc (hl)"},
	[0x0f] = {.label="rrc a"},

	[0x10] = {.label="rl b"},
	[0x11] = {.label="rl c"},
	[0x12] = {.label="rl d"},
	[0x13] = {.label="rl e"},
	[0x14] = {.label="rl h"},
	[0x15] = {.label="rl l"},
	[0x16] = {.label="rl (hl)"},
	[0x17] = {.label="rl a"},
	[0x18] = {.label="rr b"},
	[0x19] = {.label="rr c"},
	[0x1a] = {.label="rr d"},
	[0x1b] = {.label="rr e"},
	[0x1c] = {.label="rr h"},
	[0x1d] = {.label="rr l"},
	[0x1e] = {.label="rr (hl)"},
	[0x1f] = {.label="rr a"},

	[0x20] = {.label="sla b"},
	[0x21] = {.label="sla c"},
	[0x22] = {.label="sla d"},
	[0x23] = {.label="sla e"},
	[0x24] = {.label="sla h"},
	[0x25] = {.label="sla l"},
	[0x26] = {.label="sla (hl)"},
	[0x27] = {.label="sla a"},
	[0x28] = {.label="sra b"},
	[0x29] = {.label="sra c"},
	[0x2a] = {.label="sra d"},
	[0x2b] = {.label="sra e"},
	[0x2c] = {.label="sra h"},
	[0x2d] = {.label="sra l"},
	[0x2e] = {.label="sra (hl)"},
	[0x2f] = {.label="sra a"},

	[0x30] = {.label="swap b"},
	[0x31] = {.label="swap c"},
	[0x32] = {.label="swap d"},
	[0x33] = {.label="swap e"},
	[0x34] = {.label="swap h"},
	[0x35] = {.label="swap l"},
	[0x36] = {.label="swap (hl)"},
	[0x37] = {.label="swap a"},
	[0x38] = {.label="slr b"},
	[0x39] = {.label="slr c"},
	[0x3a] = {.label="slr d"},
	[0x3b] = {.label="slr e"},
	[0x3c] = {.label="slr h"},
	[0x3d] = {.label="slr l"},
	[0x3e] = {.label="slr (hl)"},
	[0x3f] = {.label="slr a"},

	[0x40] = {.label="bit 0,b"},
	[0x41] = {.label="bit 0,c"},
	[0x42] = {.label="bit 0,d"},
	[0x43] = {.label="bit 0,e"},
	[0x44] = {.label="bit 0,h"},
	[0x45] = {.label="bit 0,l"},
	[0x46] = {.label="bit 0,(hl)"},
	[0x47] = {.label="bit 0,a"},
	[0x48] = {.label="bit 1,b"},
	[0x49] = {.label="bit 1,c"},
	[0x4a] = {.label="bit 1,d"},
	[0x4b] = {.label="bit 1,e"},
	[0x4c] = {.label="bit 1,h"},
	[0x4d] = {.label="bit 1,l"},
	[0x4e] = {.label="bit 1,(hl)"},
	[0x4f] = {.label="bit 1,a"},

	[0x50] = {.label="bit 2,b"},
	[0x51] = {.label="bit 2,c"},
	[0x52] = {.label="bit 2,d"},
	[0x53] = {.label="bit 2,e"},
	[0x54] = {.label="bit 2,h"},
	[0x55] = {.label="bit 2,l"},
	[0x56] = {.label="bit 2,(hl)"},
	[0x57] = {.label="bit 2,a"},
	[0x58] = {.label="bit 3,b"},
	[0x59] = {.label="bit 3,c"},
	[0x5a] = {.label="bit 3,d"},
	[0x5b] = {.label="bit 3,e"},
	[0x5c] = {.label="bit 3,h"},
	[0x5d] = {.label="bit 3,l"},
	[0x5e] = {.label="bit 3,(hl)"},
	[0x5f] = {.label="bit 3,a"},

	[0x60] = {.label="bit 4,b"},
	[0x61] = {.label="bit 4,c"},
	[0x62] = {.label="bit 4,d"},
	[0x63] = {.label="bit 4,e"},
	[0x64] = {.label="bit 4,h"},
	[0x65] = {.label="bit 4,l"},
	[0x66] = {.label="bit 4,(hl)"},
	[0x67] = {.label="bit 4,a"},
	[0x68] = {.label="bit 5,b"},
	[0x69] = {.label="bit 5,c"},
	[0x6a] = {.label="bit 5,d"},
	[0x6b] = {.label="bit 5,e"},
	[0x6c] = {.label="bit 5,h"},
	[0x6d] = {.label="bit 5,l"},
	[0x6e] = {.label="bit 5,(hl)"},
	[0x6f] = {.label="bit 5,a"},

	[0x70] = {.label="bit 6,b"},
	[0x71] = {.label="bit 6,c"},
	[0x72] = {.label="bit 6,d"},
	[0x73] = {.label="bit 6,e"},
	[0x74] = {.label="bit 6,h"},
	[0x75] = {.label="bit 6,l"},
	[0x76] = {.label="bit 6,(hl)"},
	[0x77] = {.label="bit 6,a"},
	[0x78] = {.label="bit 7,b"},
	[0x79] = {.label="bit 7,c"},
	[0x7a] = {.label="bit 7,d"},
	[0x7b] = {.label="bit 7,e"},
	[0x7c] = {.label="bit 7,h"},
	[0x7d] = {.label="bit 7,l"},
	[0x7e] = {.label="bit 7,(hl)"},
	[0x7f] = {.label="bit 7,a"},

	[0x80] = {.label="res 0,b"},
	[0x81] = {.label="res 0,c"},
	[0x82] = {.label="res 0,d"},
	[0x83] = {.label="res 0,e"},
	[0x84] = {.label="res 0,h"},
	[0x85] = {.label="res 0,l"},
	[0x86] = {.label="res 0,(hl)"},
	[0x87] = {.label="res 0,a"},
	[0x88] = {.label="res 1,b"},
	[0x89] = {.label="res 1,c"},
	[0x8a] = {.label="res 1,d"},
	[0x8b] = {.label="res 1,e"},
	[0x8c] = {.label="res 1,h"},
	[0x8d] = {.label="res 1,l"},
	[0x8e] = {.label="res 1,(hl)"},
	[0x8f] = {.label="res 1,a"},

	[0x90] = {.label="res 2,b"},
	[0x91] = {.label="res 2,c"},
	[0x92] = {.label="res 2,d"},
	[0x93] = {.label="res 2,e"},
	[0x94] = {.label="res 2,h"},
	[0x95] = {.label="res 2,l"},
	[0x96] = {.label="res 2,(hl)"},
	[0x97] = {.label="res 2,a"},
	[0x98] = {.label="res 3,b"},
	[0x99] = {.label="res 3,c"},
	[0x9a] = {.label="res 3,d"},
	[0x9b] = {.label="res 3,e"},
	[0x9c] = {.label="res 3,h"},
	[0x9d] = {.label="res 3,l"},
	[0x9e] = {.label="res 3,(hl)"},
	[0x9f] = {.label="res 3,a"},

	[0xa0] = {.label="res 4,b"},
	[0xa1] = {.label="res 4,c"},
	[0xa2] = {.label="res 4,d"},
	[0xa3] = {.label="res 4,e"},
	[0xa4] = {.label="res 4,h"},
	[0xa5] = {.label="res 4,l"},
	[0xa6] = {.label="res 4,(hl)"},
	[0xa7] = {.label="res 4,a"},
	[0xa8] = {.label="res 5,b"},
	[0xa9] = {.label="res 5,c"},
	[0xaa] = {.label="res 5,d"},
	[0xab] = {.label="res 5,e"},
	[0xac] = {.label="res 5,h"},
	[0xad] = {.label="res 5,l"},
	[0xae] = {.label="res 5,(hl)"},
	[0xaf] = {.label="res 5,a"},

	[0xb0] = {.label="res 6,b"},
	[0xb1] = {.label="res 6,c"},
	[0xb2] = {.label="res 6,d"},
	[0xb3] = {.label="res 6,e"},
	[0xb4] = {.label="res 6,h"},
	[0xb5] = {.label="res 6,l"},
	[0xb6] = {.label="res 6,(hl)"},
	[0xb7] = {.label="res 6,a"},
	[0xb8] = {.label="res 7,b"},
	[0xb9] = {.label="res 7,c"},
	[0xba] = {.label="res 7,d"},
	[0xbb] = {.label="res 7,e"},
	[0xbc] = {.label="res 7,h"},
	[0xbd] = {.label="res 7,l"},
	[0xbe] = {.label="res 7,(hl)"},
	[0xbf] = {.label="res 7,a"},

	[0xc0] = {.label="set 0,b"},
	[0xc1] = {.label="set 0,c"},
	[0xc2] = {.label="set 0,d"},
	[0xc3] = {.label="set 0,e"},
	[0xc4] = {.label="set 0,h"},
	[0xc5] = {.label="set 0,l"},
	[0xc6] = {.label="set 0,(hl)"},
	[0xc7] = {.label="set 0,a"},
	[0xc8] = {.label="set 1,b"},
	[0xc9] = {.label="set 1,c"},
	[0xca] = {.label="set 1,d"},
	[0xcb] = {.label="set 1,e"},
	[0xcc] = {.label="set 1,h"},
	[0xcd] = {.label="set 1,l"},
	[0xce] = {.label="set 1,(hl)"},
	[0xcf] = {.label="set 1,a"},

	[0xd0] = {.label="set 2,b"},
	[0xd1] = {.label="set 2,c"},
	[0xd2] = {.label="set 2,d"},
	[0xd3] = {.label="set 2,e"},
	[0xd4] = {.label="set 2,h"},
	[0xd5] = {.label="set 2,l"},
	[0xd6] = {.label="set 2,(hl)"},
	[0xd7] = {.label="set 2,a"},
	[0xd8] = {.label="set 3,b"},
	[0xd9] = {.label="set 3,c"},
	[0xda] = {.label="set 3,d"},
	[0xdb] = {.label="set 3,e"},
	[0xdc] = {.label="set 3,h"},
	[0xdd] = {.label="set 3,l"},
	[0xde] = {.label="set 3,(hl)"},
	[0xdf] = {.label="set 3,a"},

	[0xe0] = {.label="set 4,b"},
	[0xe1] = {.label="set 4,c"},
	[0xe2] = {.label="set 4,d"},
	[0xe3] = {.label="set 4,e"},
	[0xe4] = {.label="set 4,h"},
	[0xe5] = {.label="set 4,l"},
	[0xe6] = {.label="set 4,(hl)"},
	[0xe7] = {.label="set 4,a"},
	[0xe8] = {.label="set 5,b"},
	[0xe9] = {.label="set 5,c"},
	[0xea] = {.label="set 5,d"},
	[0xeb] = {.label="set 5,e"},
	[0xec] = {.label="set 5,h"},
	[0xed] = {.label="set 5,l"},
	[0xee] = {.label="set 5,(hl)"},
	[0xef] = {.label="set 5,a"},

	[0xf0] = {.label="set 6,b"},
	[0xf1] = {.label="set 6,c"},
	[0xf2] = {.label="set 6,d"},
	[0xf3] = {.label="set 6,e"},
	[0xf4] = {.label="set 6,h"},
	[0xf5] = {.label="set 6,l"},
	[0xf6] = {.label="set 6,(hl)"},
	[0xf7] = {.label="set 6,a"},
	[0xf8] = {.label="set 7,b"},
	[0xf9] = {.label="set 7,c"},
	[0xfa] = {.label="set 7,d"},
	[0xfb] = {.label="set 7,e"},
	[0xfc] = {.label="set 7,h"},
	[0xfd] = {.label="set 7,l"},
	[0xfe] = {.label="set 7,(hl)"},
	[0xff] = {.label="set 7,a"},
};

#define HEX_DEFAULT	"%02X               "
#define HEX_NINTENDO	"%02X %02X %02X %02X+     db "
#define HEX_DB_CHAR	"%02X               db %02X"
#define HEX_DB_WORD	"%02X %02X            db %02X,%02X"

#define JR_INS		"\x18\x20\x28\x30\x38"
#define JP_INS		"\xc2\xc3\xd2\xda\xe9"
#define CALL_INS	"\xc4\xcc\xcd\xd4\xdc"
enum {JMP_JR, JMP_JP, JMP_CALL};

struct label {
	u16 from;
	u16 to;
	int type;
	struct label *next;
};

enum {
	NO_FLAGS = 0,
	FORCE_RETURN_CARRIAGE = 1 << 0
};

struct line {
	int addr;
	char buff[128];
	struct line *next;
	unsigned char flags;
};

static int pc = 0x0000;
static struct line *lines;
static struct label *labels;

static void add_label(u16 from, u16 to, int type)
{
	struct label *lbl = malloc(sizeof(*lbl));

	lbl->from = from;
	lbl->to = to;
	lbl->type = type;
	lbl->next = labels;
	labels = lbl;
}

static void format_line(struct line *line, char *fmt, ...)
{
	va_list va;
	int bank = pc / 0x4000;
	int ins_addr = pc;

	va_start(va, fmt);
	if (bank)
		ins_addr = pc % 0x4000 + 0x4000;
	line->addr = ins_addr;
	sprintf(line->buff, "%s%X:%04X %02X", (bank < 0x10) ? "ROM" : "RO", bank, ins_addr, pc);
	vsprintf(&line->buff[10], fmt, va);
	va_end(va);
}

static struct line *next_ins(u8 *stream, struct line *prev_line)
{
	char *label;
	int type;
	u8 ins;
	struct line *line = calloc(sizeof(*line), 1);
	char *linebuff = line->buff;
	struct instruction *ins_set = default_ins_set;

	if (prev_line)
		prev_line->next = line;

	/* GameBoy cartridges specific header */
	if (pc <= 0x014E && pc >= 0x0104) {
		u8 *db = &stream[pc];

		/* Nintendo pic + ROM name */
		if (pc < 0x0144) {
			unsigned int n;
			format_line(line, HEX_NINTENDO, db[0], db[1], db[2], db[3]);
			for (n = 0; n < 16; n++)
				snprintf(&linebuff[30 + n * 3], 4, "%02X,", db[n]);
			linebuff[30 + 16 * 3 - 1] = '\0';
			pc += 16;
			goto end;
		}

		/* Unused */
		if (pc < 0x0146) {
			format_line(line, HEX_DB_WORD, db[0], db[1], db[0], db[1]);
			pc += 2;
			goto end;
		}

		/* Cartridge type, ROM size, RAM size, Manufacturer code, Version number, Complement check */
		if (pc < 0x014E) {
			format_line(line, HEX_DB_CHAR, db[0], db[0]);
			pc++;
			goto end;
		}

		/* Checksum */
		format_line(line, HEX_DB_WORD, db[0], db[1], db[0], db[1]);
		pc += 2;
		goto end;
	}

	ins = stream[pc];

	format_line(line, HEX_DEFAULT, ins);
	pc++;

	if (ins == 0xC9) // ret
		line->flags |= FORCE_RETURN_CARRIAGE;
	else if (ins == 0xCB) { // extended set
		ins_set = extended_ins_set;
		ins = stream[pc++];
		sprintf(&linebuff[13], "%02X", ins);
		linebuff[15] = ' ';
	}
	label = ins_set[ins].label;
	type = ins_set[ins].type;

	if (!label) {
		sprintf(&linebuff[27], "undefined opcode");
		goto end;
	}

	switch (type) {
	case P_WORD:
	{
		u16 p = GET_ADDR(pc);
		sprintf(&linebuff[13], "%02X %02X", stream[pc], stream[pc + 1]);
		linebuff[18] = ' ';
		sprintf(&linebuff[27], label, p);
		if (ins_set == default_ins_set) {
			if (index(CALL_INS, ins))
				add_label(REL_ADDR(pc - 1), p, JMP_CALL);
			else if (index(JP_INS, ins))
				add_label(REL_ADDR(pc - 1), p, JMP_JP);
		}
		pc += 2;
		break;
	}

	case P_UCHAR8:
	{
		u8 p = stream[pc++];
		sprintf(&linebuff[13], "%02X", p);
		linebuff[15] = ' ';
		sprintf(&linebuff[27], label, p);
		break;
	}

	case P_CHAR8:
	{
		int8_t p = stream[pc];
		int rom_addr = pc + p + 1;
		u16 addr = REL_ADDR(rom_addr);
		sprintf(&linebuff[13], "%02X", (u8)p);
		linebuff[15] = ' ';
		sprintf(&linebuff[27], label, addr);
		if (ins_set == default_ins_set && index(JR_INS, ins))
			add_label(REL_ADDR(pc - 1), addr, JMP_JR);
		pc++;
		break;
	}

	case P_NONE:
		sprintf(&linebuff[27], "%s", label);
		break;
	}

end:
	return line;
}

static char *get_type_str(int type) {
	switch (type) {
	case JMP_JR:
		return "jr";
	case JMP_JP:
		return "jp";
	case JMP_CALL:
		return "call";
	}
	return NULL;
}

struct hexbuffer {
	char *buffer;
	int size;
	int i;
};

#define BUFFERING 102400
#define TRY_APPEND do {\
	va_start(va, fmt);\
	ret = vsnprintf(&hex->buffer[hex->i], hex->size - hex->i, fmt, va);\
	if (ret < 0)\
		goto end;\
} while(0)

static void hexbuffer_append(struct hexbuffer *hex, char *fmt, ...)
{
	va_list va;
	int ret;

	TRY_APPEND;
	while (hex->i + ret >= hex->size) {
		hex->buffer = realloc(hex->buffer, hex->size + BUFFERING);
		hex->size += BUFFERING;
		TRY_APPEND;
	}
	hex->i += ret;
end:
	va_end(va);
}

static PyObject *get_buffer(void)
{
	struct line *line = lines;
	PyObject *buffer;
	struct hexbuffer hex = {.buffer = malloc(BUFFERING), .size = BUFFERING, .i = 0};
	*hex.buffer = 0;
	int ihazret = 0;

	while (line) {
		struct line *old_line;
		struct label *lbl = labels, *old_lbl = NULL;
		int first = 1;

		while (lbl) {
			if (lbl->to == line->addr) {
				struct label *next = lbl->next;

				if (first) {
					first = 0;
					hexbuffer_append(&hex, "%s; Jump here from: %04X (%s)",
							ihazret ? "": "\n", lbl->from, get_type_str(lbl->type));
				} else {
					hexbuffer_append(&hex, ", %04X (%s)", lbl->from, get_type_str(lbl->type));
				}
				ihazret = 0;
				free(lbl);
				if (!old_lbl)
					labels = next;
				else
					old_lbl->next = next;
				lbl = next;
				continue;
			}
			old_lbl = lbl;
			lbl = lbl->next;
		}
		if (!first)
			hexbuffer_append(&hex, "\n");
		ihazret = line->flags & FORCE_RETURN_CARRIAGE;
		hexbuffer_append(&hex, "%s%s", line->buff, ihazret ? "\n\n" :"\n");
		old_line = line;
		line = line->next;
		free(old_line);
	}
	buffer = Py_BuildValue("s", hex.buffer);
	free(hex.buffer);
	return buffer;
}

PyObject *disasm(struct rom *self, PyObject *args)
{
	int bank_id = 0;
	struct line *line;

	PyArg_ParseTuple(args, "i", &bank_id);
	pc = bank_id * 0x4000;
	lines = line = next_ins(self->stream, NULL);
	while (pc < (bank_id + 1) * 0x4000)
		line = next_ins(self->stream, line);
	return get_buffer();
}
