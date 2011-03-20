/*
 *  This file is part of Pokanalysis.
 *
 *  Pokanalysis
 *  Copyright © 2010 Clément Bœsch
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

enum {P_NONE, P_UCHAR8, P_CHAR8, P_WORD};

static struct instruction default_ins_set[] = {
	[0x00] = {"nop", P_NONE},
	[0x01] = {"ld bc,%04x", P_WORD},
	[0x02] = {"ld (bc),a", P_NONE},
	[0x03] = {"inc bc", P_NONE},
	[0x04] = {"inc b", P_NONE},
	[0x05] = {"dec b", P_NONE},
	[0x06] = {"ld b,%02x", P_UCHAR8},
	[0x07] = {"rlca", P_NONE},
	[0x08] = {"ld (%04x),sp", P_WORD},
	[0x09] = {"add hl,bc", P_NONE},
	[0x0a] = {"ld a,(bc)", P_NONE},
	[0x0b] = {"dec bc", P_NONE},
	[0x0c] = {"inc c", P_NONE},
	[0x0d] = {"dec c", P_NONE},
	[0x0e] = {"ld c,%02x", P_UCHAR8},
	[0x0f] = {"rrca", P_NONE},

	[0x10] = {"stop", P_NONE}, /* 0x10 0x00 */
	[0x11] = {"ld de,%04x", P_WORD},
	[0x12] = {"ld (de),a", P_NONE},
	[0x1a] = {"ld a,(de)", P_NONE},
	[0x1b] = {"dec de", P_NONE},
	[0x13] = {"inc de", P_NONE},
	[0x14] = {"inc d", P_NONE},
	[0x15] = {"dec d", P_NONE},
	[0x16] = {"ld d,%02x", P_UCHAR8},
	[0x17] = {"rla", P_NONE},
	[0x18] = {"jr %04x", P_CHAR8},
	[0x19] = {"add hl,de", P_NONE},
	[0x1c] = {"inc e", P_NONE},
	[0x1d] = {"dec e", P_NONE},
	[0x1e] = {"ld e,%02x", P_UCHAR8},
	[0x1f] = {"rra", P_NONE},

	[0x20] = {"jr nz,%04x", P_CHAR8},
	[0x21] = {"ld hl,%04x", P_WORD},
	[0x22] = {"ldi (hl),a", P_NONE},
	[0x23] = {"inc hl", P_NONE},
	[0x24] = {"inc h", P_NONE},
	[0x25] = {"dec h", P_NONE},
	[0x27] = {"daa", P_NONE},
	[0x28] = {"jr z,%04x", P_CHAR8},
	[0x2a] = {"ldi a,(hl)", P_NONE},
	[0x2b] = {"dec hl", P_NONE},
	[0x26] = {"ld h,%02x", P_UCHAR8},
	[0x29] = {"add hl,hl", P_NONE},
	[0x2c] = {"inc l", P_NONE},
	[0x2d] = {"dec l", P_NONE},
	[0x2e] = {"ld l,%02x", P_UCHAR8},
	[0x2f] = {"cpl", P_NONE},

	[0x30] = {"jr nc,%04x", P_CHAR8},
	[0x31] = {"ld sp,%04x", P_WORD},
	[0x32] = {"ldd (hl),a", P_NONE},
	[0x33] = {"inc sp", P_NONE},
	[0x34] = {"inc (hl)", P_NONE},
	[0x35] = {"dec (hl)", P_NONE},
	[0x37] = {"scf", P_NONE},
	[0x38] = {"jr c,%04x", P_CHAR8},
	[0x3a] = {"ldd a,(hl)", P_NONE},
	[0x3b] = {"dec sp", P_NONE},
	[0x3c] = {"inc a", P_NONE},
	[0x3d] = {"dec a", P_NONE},
	[0x36] = {"ld (hl),%02x", P_UCHAR8},
	[0x39] = {"add hl,sp", P_NONE},
	[0x3e] = {"ld a,%02x", P_UCHAR8},
	[0x3f] = {"ccf", P_NONE},

	[0x40] = {"ld b,b", P_NONE},
	[0x41] = {"ld b,c", P_NONE},
	[0x42] = {"ld b,d", P_NONE},
	[0x43] = {"ld b,e", P_NONE},
	[0x44] = {"ld b,h", P_NONE},
	[0x45] = {"ld b,l", P_NONE},
	[0x46] = {"ld b,(hl)", P_NONE},
	[0x47] = {"ld b,a", P_NONE},

	[0x48] = {"ld c,b", P_NONE},
	[0x49] = {"ld c,c", P_NONE},
	[0x4a] = {"ld c,d", P_NONE},
	[0x4b] = {"ld c,e", P_NONE},
	[0x4c] = {"ld c,h", P_NONE},
	[0x4d] = {"ld c,l", P_NONE},
	[0x4e] = {"ld c,(hl)", P_NONE},
	[0x4f] = {"ld c,a", P_NONE},

	[0x50] = {"ld d,b", P_NONE},
	[0x51] = {"ld d,c", P_NONE},
	[0x52] = {"ld d,d", P_NONE},
	[0x53] = {"ld d,e", P_NONE},
	[0x54] = {"ld d,h", P_NONE},
	[0x55] = {"ld d,l", P_NONE},
	[0x56] = {"ld d,(hl)", P_NONE},
	[0x57] = {"ld d,a", P_NONE},

	[0x58] = {"ld e,b", P_NONE},
	[0x59] = {"ld e,c", P_NONE},
	[0x5a] = {"ld e,d", P_NONE},
	[0x5b] = {"ld e,e", P_NONE},
	[0x5c] = {"ld e,h", P_NONE},
	[0x5d] = {"ld e,l", P_NONE},
	[0x5e] = {"ld e,(hl)", P_NONE},
	[0x5f] = {"ld e,a", P_NONE},

	[0x60] = {"ld h,b", P_NONE},
	[0x61] = {"ld h,c", P_NONE},
	[0x62] = {"ld h,d", P_NONE},
	[0x63] = {"ld h,e", P_NONE},
	[0x64] = {"ld h,h", P_NONE},
	[0x65] = {"ld h,l", P_NONE},
	[0x66] = {"ld h,(hl)", P_NONE},
	[0x67] = {"ld h,a", P_NONE},

	[0x68] = {"ld l,b", P_NONE},
	[0x69] = {"ld l,c", P_NONE},
	[0x6a] = {"ld l,d", P_NONE},
	[0x6b] = {"ld l,e", P_NONE},
	[0x6c] = {"ld l,h", P_NONE},
	[0x6d] = {"ld l,l", P_NONE},
	[0x6e] = {"ld l,(hl)", P_NONE},
	[0x6f] = {"ld l,a", P_NONE},

	[0x70] = {"ld (hl),b", P_NONE},
	[0x71] = {"ld (hl),c", P_NONE},
	[0x72] = {"ld (hl),d", P_NONE},
	[0x73] = {"ld (hl),e", P_NONE},
	[0x74] = {"ld (hl),h", P_NONE},
	[0x75] = {"ld (hl),l", P_NONE},
	[0x76] = {"halt", P_NONE},
	[0x77] = {"ld (hl),a", P_NONE},

	[0x78] = {"ld a,b", P_NONE},
	[0x79] = {"ld a,c", P_NONE},
	[0x7a] = {"ld a,d", P_NONE},
	[0x7b] = {"ld a,e", P_NONE},
	[0x7c] = {"ld a,h", P_NONE},
	[0x7d] = {"ld a,l", P_NONE},
	[0x7e] = {"ld a,(hl)", P_NONE},
	[0x7f] = {"ld a,a", P_NONE},

	[0x80] = {"add b", P_NONE},
	[0x81] = {"add c", P_NONE},
	[0x82] = {"add d", P_NONE},
	[0x83] = {"add e", P_NONE},
	[0x84] = {"add h", P_NONE},
	[0x85] = {"add l", P_NONE},
	[0x86] = {"add (hl)", P_NONE},
	[0x87] = {"add a", P_NONE},
	[0x88] = {"adc b", P_NONE},
	[0x89] = {"adc c", P_NONE},
	[0x8a] = {"adc d", P_NONE},
	[0x8b] = {"adc e", P_NONE},
	[0x8c] = {"adc h", P_NONE},
	[0x8d] = {"adc l", P_NONE},
	[0x8e] = {"adc (hl)", P_NONE},
	[0x8f] = {"adc a", P_NONE},

	[0x90] = {"sub b", P_NONE},
	[0x91] = {"sub c", P_NONE},
	[0x92] = {"sub d", P_NONE},
	[0x93] = {"sub e", P_NONE},
	[0x94] = {"sub h", P_NONE},
	[0x95] = {"sub l", P_NONE},
	[0x96] = {"sub (hl)", P_NONE},
	[0x97] = {"sub a", P_NONE},
	[0x98] = {"sbc b", P_NONE},
	[0x99] = {"sbc c", P_NONE},
	[0x9a] = {"sbc d", P_NONE},
	[0x9b] = {"sbc e", P_NONE},
	[0x9c] = {"sbc h", P_NONE},
	[0x9d] = {"sbc l", P_NONE},
	[0x9e] = {"sbc (hl)", P_NONE},
	[0x9f] = {"sbc a", P_NONE},

	[0xa0] = {"and b", P_NONE},
	[0xa1] = {"and c", P_NONE},
	[0xa2] = {"and d", P_NONE},
	[0xa3] = {"and e", P_NONE},
	[0xa4] = {"and h", P_NONE},
	[0xa5] = {"and l", P_NONE},
	[0xa6] = {"and (hl)", P_NONE},
	[0xa7] = {"and a", P_NONE},
	[0xa8] = {"xor b", P_NONE},
	[0xa9] = {"xor c", P_NONE},
	[0xaa] = {"xor d", P_NONE},
	[0xab] = {"xor e", P_NONE},
	[0xac] = {"xor h", P_NONE},
	[0xad] = {"xor l", P_NONE},
	[0xae] = {"xor (hl)", P_NONE},
	[0xaf] = {"xor a", P_NONE},

	[0xb0] = {"or b", P_NONE},
	[0xb1] = {"or c", P_NONE},
	[0xb2] = {"or d", P_NONE},
	[0xb3] = {"or e", P_NONE},
	[0xb4] = {"or h", P_NONE},
	[0xb5] = {"or l", P_NONE},
	[0xb6] = {"or (hl)", P_NONE},
	[0xb7] = {"or a", P_NONE},
	[0xb8] = {"cp b", P_NONE},
	[0xb9] = {"cp c", P_NONE},
	[0xba] = {"cp d", P_NONE},
	[0xbb] = {"cp e", P_NONE},
	[0xbc] = {"cp h", P_NONE},
	[0xbd] = {"cp l", P_NONE},
	[0xbe] = {"cp (hl)", P_NONE},
	[0xbf] = {"cp a", P_NONE},

	[0xc0] = {"ret nz", P_NONE},
	[0xc1] = {"pop bc", P_NONE},
	[0xc2] = {"jp nz %04x", P_WORD},
	[0xc3] = {"jp %04x", P_WORD},
	[0xc4] = {"call nz %04x", P_WORD},
	[0xc5] = {"push bc", P_NONE},
	[0xc6] = {"add a,%02x", P_UCHAR8},
	[0xc7] = {"rst 00", P_NONE},
	[0xc8] = {"ret z", P_NONE},
	[0xc9] = {"ret", P_NONE},
	[0xca] = {"jp z,%04x", P_WORD},

	/* 0xCB: Extended opcode */

	[0xcc] = {"call z,%04x", P_WORD},
	[0xcd] = {"call %04x", P_WORD},
	[0xce] = {"adc a,%02x", P_UCHAR8},
	[0xcf] = {"rst 08", P_NONE},

	[0xd0] = {"ret nc", P_NONE},
	[0xd1] = {"pop de", P_NONE},
	[0xd2] = {"jp nc %04x", P_WORD},

	[0xd4] = {"call nc %04x", P_WORD},
	[0xd5] = {"push de", P_NONE},
	[0xd6] = {"sub a,%02x", P_UCHAR8},
	[0xd7] = {"rst 10", P_NONE},
	[0xd8] = {"ret c", P_NONE},
	[0xd9] = {"reti", P_NONE},
	[0xda] = {"jp c,%04x", P_WORD},

	[0xdc] = {"call c,%04x", P_WORD},

	[0xde] = {"sbc a,%02x", P_UCHAR8},
	[0xdf] = {"rst 18", P_NONE},

	[0xe0] = {"ld (ff00+%02x),a", P_UCHAR8},
	[0xe1] = {"pop hl", P_NONE},
	[0xe2] = {"ld (ff00+c),a", P_NONE},

	[0xe5] = {"push hl", P_NONE},
	[0xe6] = {"and a,%02x", P_UCHAR8},
	[0xe7] = {"rst 20", P_NONE},
	[0xe8] = {"add sp,%02x", P_UCHAR8},
	[0xe9] = {"jp hl", P_NONE},
	[0xea] = {"ld (%04x),a", P_WORD},

	[0xee] = {"xor a,%02x", P_UCHAR8},
	[0xef] = {"rst 28", P_NONE},

	[0xf0] = {"ld a,(ff00+%02x)", P_UCHAR8},
	[0xf1] = {"pop af", P_NONE},
	[0xf2] = {"ld a,(ff00+c)", P_NONE},
	[0xf3] = {"di", P_NONE},

	[0xf5] = {"push af", P_NONE},
	[0xf6] = {"or a,%02x", P_UCHAR8},
	[0xf7] = {"rst 30", P_NONE},
	[0xf8] = {"ld hl,sp+%02x", P_UCHAR8},
	[0xf9] = {"ld sp,hl", P_NONE},
	[0xfa] = {"ld a,(%04x)", P_WORD},
	[0xfb] = {"ei", P_NONE},

	[0xfe] = {"cp a,%02x", P_UCHAR8},
	[0xff] = {"rst 38", P_NONE},
};

static struct instruction extended_ins_set[] = {
	[0x00] = {"rlc b", P_NONE},
	[0x01] = {"rlc c", P_NONE},
	[0x02] = {"rlc d", P_NONE},
	[0x03] = {"rlc e", P_NONE},
	[0x04] = {"rlc h", P_NONE},
	[0x05] = {"rlc l", P_NONE},
	[0x06] = {"rlc (hl)", P_NONE},
	[0x07] = {"rlc a", P_NONE},
	[0x08] = {"rrc b", P_NONE},
	[0x09] = {"rrc c", P_NONE},
	[0x0a] = {"rrc d", P_NONE},
	[0x0b] = {"rrc e", P_NONE},
	[0x0c] = {"rrc h", P_NONE},
	[0x0d] = {"rrc l", P_NONE},
	[0x0e] = {"rrc (hl)", P_NONE},
	[0x0f] = {"rrc a", P_NONE},

	[0x10] = {"rl b", P_NONE},
	[0x11] = {"rl c", P_NONE},
	[0x12] = {"rl d", P_NONE},
	[0x13] = {"rl e", P_NONE},
	[0x14] = {"rl h", P_NONE},
	[0x15] = {"rl l", P_NONE},
	[0x16] = {"rl (hl)", P_NONE},
	[0x17] = {"rl a", P_NONE},
	[0x18] = {"rr b", P_NONE},
	[0x19] = {"rr c", P_NONE},
	[0x1a] = {"rr d", P_NONE},
	[0x1b] = {"rr e", P_NONE},
	[0x1c] = {"rr h", P_NONE},
	[0x1d] = {"rr l", P_NONE},
	[0x1e] = {"rr (hl)", P_NONE},
	[0x1f] = {"rr a", P_NONE},

	[0x20] = {"sla b", P_NONE},
	[0x21] = {"sla c", P_NONE},
	[0x22] = {"sla d", P_NONE},
	[0x23] = {"sla e", P_NONE},
	[0x24] = {"sla h", P_NONE},
	[0x25] = {"sla l", P_NONE},
	[0x26] = {"sla (hl)", P_NONE},
	[0x27] = {"sla a", P_NONE},
	[0x28] = {"sra b", P_NONE},
	[0x29] = {"sra c", P_NONE},
	[0x2a] = {"sra d", P_NONE},
	[0x2b] = {"sra e", P_NONE},
	[0x2c] = {"sra h", P_NONE},
	[0x2d] = {"sra l", P_NONE},
	[0x2e] = {"sra (hl)", P_NONE},
	[0x2f] = {"sra a", P_NONE},

	[0x30] = {"swap b", P_NONE},
	[0x31] = {"swap c", P_NONE},
	[0x32] = {"swap d", P_NONE},
	[0x33] = {"swap e", P_NONE},
	[0x34] = {"swap h", P_NONE},
	[0x35] = {"swap l", P_NONE},
	[0x36] = {"swap (hl)", P_NONE},
	[0x37] = {"swap a", P_NONE},
	[0x38] = {"slr b", P_NONE},
	[0x39] = {"slr c", P_NONE},
	[0x3a] = {"slr d", P_NONE},
	[0x3b] = {"slr e", P_NONE},
	[0x3c] = {"slr h", P_NONE},
	[0x3d] = {"slr l", P_NONE},
	[0x3e] = {"slr (hl)", P_NONE},
	[0x3f] = {"slr a", P_NONE},

	[0x40] = {"bit 0,b", P_NONE},
	[0x41] = {"bit 0,c", P_NONE},
	[0x42] = {"bit 0,d", P_NONE},
	[0x43] = {"bit 0,e", P_NONE},
	[0x44] = {"bit 0,h", P_NONE},
	[0x45] = {"bit 0,l", P_NONE},
	[0x46] = {"bit 0,(hl)", P_NONE},
	[0x47] = {"bit 0,a", P_NONE},
	[0x48] = {"bit 1,b", P_NONE},
	[0x49] = {"bit 1,c", P_NONE},
	[0x4a] = {"bit 1,d", P_NONE},
	[0x4b] = {"bit 1,e", P_NONE},
	[0x4c] = {"bit 1,h", P_NONE},
	[0x4d] = {"bit 1,l", P_NONE},
	[0x4e] = {"bit 1,(hl)", P_NONE},
	[0x4f] = {"bit 1,a", P_NONE},

	[0x50] = {"bit 2,b", P_NONE},
	[0x51] = {"bit 2,c", P_NONE},
	[0x52] = {"bit 2,d", P_NONE},
	[0x53] = {"bit 2,e", P_NONE},
	[0x54] = {"bit 2,h", P_NONE},
	[0x55] = {"bit 2,l", P_NONE},
	[0x56] = {"bit 2,(hl)", P_NONE},
	[0x57] = {"bit 2,a", P_NONE},
	[0x58] = {"bit 3,b", P_NONE},
	[0x59] = {"bit 3,c", P_NONE},
	[0x5a] = {"bit 3,d", P_NONE},
	[0x5b] = {"bit 3,e", P_NONE},
	[0x5c] = {"bit 3,h", P_NONE},
	[0x5d] = {"bit 3,l", P_NONE},
	[0x5e] = {"bit 3,(hl)", P_NONE},
	[0x5f] = {"bit 3,a", P_NONE},

	[0x60] = {"bit 4,b", P_NONE},
	[0x61] = {"bit 4,c", P_NONE},
	[0x62] = {"bit 4,d", P_NONE},
	[0x63] = {"bit 4,e", P_NONE},
	[0x64] = {"bit 4,h", P_NONE},
	[0x65] = {"bit 4,l", P_NONE},
	[0x66] = {"bit 4,(hl)", P_NONE},
	[0x67] = {"bit 4,a", P_NONE},
	[0x68] = {"bit 5,b", P_NONE},
	[0x69] = {"bit 5,c", P_NONE},
	[0x6a] = {"bit 5,d", P_NONE},
	[0x6b] = {"bit 5,e", P_NONE},
	[0x6c] = {"bit 5,h", P_NONE},
	[0x6d] = {"bit 5,l", P_NONE},
	[0x6e] = {"bit 5,(hl)", P_NONE},
	[0x6f] = {"bit 5,a", P_NONE},

	[0x70] = {"bit 6,b", P_NONE},
	[0x71] = {"bit 6,c", P_NONE},
	[0x72] = {"bit 6,d", P_NONE},
	[0x73] = {"bit 6,e", P_NONE},
	[0x74] = {"bit 6,h", P_NONE},
	[0x75] = {"bit 6,l", P_NONE},
	[0x76] = {"bit 6,(hl)", P_NONE},
	[0x77] = {"bit 6,a", P_NONE},
	[0x78] = {"bit 7,b", P_NONE},
	[0x79] = {"bit 7,c", P_NONE},
	[0x7a] = {"bit 7,d", P_NONE},
	[0x7b] = {"bit 7,e", P_NONE},
	[0x7c] = {"bit 7,h", P_NONE},
	[0x7d] = {"bit 7,l", P_NONE},
	[0x7e] = {"bit 7,(hl)", P_NONE},
	[0x7f] = {"bit 7,a", P_NONE},

	[0x80] = {"res 0,b", P_NONE},
	[0x81] = {"res 0,c", P_NONE},
	[0x82] = {"res 0,d", P_NONE},
	[0x83] = {"res 0,e", P_NONE},
	[0x84] = {"res 0,h", P_NONE},
	[0x85] = {"res 0,l", P_NONE},
	[0x86] = {"res 0,(hl)", P_NONE},
	[0x87] = {"res 0,a", P_NONE},
	[0x88] = {"res 1,b", P_NONE},
	[0x89] = {"res 1,c", P_NONE},
	[0x8a] = {"res 1,d", P_NONE},
	[0x8b] = {"res 1,e", P_NONE},
	[0x8c] = {"res 1,h", P_NONE},
	[0x8d] = {"res 1,l", P_NONE},
	[0x8e] = {"res 1,(hl)", P_NONE},
	[0x8f] = {"res 1,a", P_NONE},

	[0x90] = {"res 2,b", P_NONE},
	[0x91] = {"res 2,c", P_NONE},
	[0x92] = {"res 2,d", P_NONE},
	[0x93] = {"res 2,e", P_NONE},
	[0x94] = {"res 2,h", P_NONE},
	[0x95] = {"res 2,l", P_NONE},
	[0x96] = {"res 2,(hl)", P_NONE},
	[0x97] = {"res 2,a", P_NONE},
	[0x98] = {"res 3,b", P_NONE},
	[0x99] = {"res 3,c", P_NONE},
	[0x9a] = {"res 3,d", P_NONE},
	[0x9b] = {"res 3,e", P_NONE},
	[0x9c] = {"res 3,h", P_NONE},
	[0x9d] = {"res 3,l", P_NONE},
	[0x9e] = {"res 3,(hl)", P_NONE},
	[0x9f] = {"res 3,a", P_NONE},

	[0xa0] = {"res 4,b", P_NONE},
	[0xa1] = {"res 4,c", P_NONE},
	[0xa2] = {"res 4,d", P_NONE},
	[0xa3] = {"res 4,e", P_NONE},
	[0xa4] = {"res 4,h", P_NONE},
	[0xa5] = {"res 4,l", P_NONE},
	[0xa6] = {"res 4,(hl)", P_NONE},
	[0xa7] = {"res 4,a", P_NONE},
	[0xa8] = {"res 5,b", P_NONE},
	[0xa9] = {"res 5,c", P_NONE},
	[0xaa] = {"res 5,d", P_NONE},
	[0xab] = {"res 5,e", P_NONE},
	[0xac] = {"res 5,h", P_NONE},
	[0xad] = {"res 5,l", P_NONE},
	[0xae] = {"res 5,(hl)", P_NONE},
	[0xaf] = {"res 5,a", P_NONE},

	[0xb0] = {"res 6,b", P_NONE},
	[0xb1] = {"res 6,c", P_NONE},
	[0xb2] = {"res 6,d", P_NONE},
	[0xb3] = {"res 6,e", P_NONE},
	[0xb4] = {"res 6,h", P_NONE},
	[0xb5] = {"res 6,l", P_NONE},
	[0xb6] = {"res 6,(hl)", P_NONE},
	[0xb7] = {"res 6,a", P_NONE},
	[0xb8] = {"res 7,b", P_NONE},
	[0xb9] = {"res 7,c", P_NONE},
	[0xba] = {"res 7,d", P_NONE},
	[0xbb] = {"res 7,e", P_NONE},
	[0xbc] = {"res 7,h", P_NONE},
	[0xbd] = {"res 7,l", P_NONE},
	[0xbe] = {"res 7,(hl)", P_NONE},
	[0xbf] = {"res 7,a", P_NONE},

	[0xc0] = {"set 0,b", P_NONE},
	[0xc1] = {"set 0,c", P_NONE},
	[0xc2] = {"set 0,d", P_NONE},
	[0xc3] = {"set 0,e", P_NONE},
	[0xc4] = {"set 0,h", P_NONE},
	[0xc5] = {"set 0,l", P_NONE},
	[0xc6] = {"set 0,(hl)", P_NONE},
	[0xc7] = {"set 0,a", P_NONE},
	[0xc8] = {"set 1,b", P_NONE},
	[0xc9] = {"set 1,c", P_NONE},
	[0xca] = {"set 1,d", P_NONE},
	[0xcb] = {"set 1,e", P_NONE},
	[0xcc] = {"set 1,h", P_NONE},
	[0xcd] = {"set 1,l", P_NONE},
	[0xce] = {"set 1,(hl)", P_NONE},
	[0xcf] = {"set 1,a", P_NONE},

	[0xd0] = {"set 2,b", P_NONE},
	[0xd1] = {"set 2,c", P_NONE},
	[0xd2] = {"set 2,d", P_NONE},
	[0xd3] = {"set 2,e", P_NONE},
	[0xd4] = {"set 2,h", P_NONE},
	[0xd5] = {"set 2,l", P_NONE},
	[0xd6] = {"set 2,(hl)", P_NONE},
	[0xd7] = {"set 2,a", P_NONE},
	[0xd8] = {"set 3,b", P_NONE},
	[0xd9] = {"set 3,c", P_NONE},
	[0xda] = {"set 3,d", P_NONE},
	[0xdb] = {"set 3,e", P_NONE},
	[0xdc] = {"set 3,h", P_NONE},
	[0xdd] = {"set 3,l", P_NONE},
	[0xde] = {"set 3,(hl)", P_NONE},
	[0xdf] = {"set 3,a", P_NONE},

	[0xe0] = {"set 4,b", P_NONE},
	[0xe1] = {"set 4,c", P_NONE},
	[0xe2] = {"set 4,d", P_NONE},
	[0xe3] = {"set 4,e", P_NONE},
	[0xe4] = {"set 4,h", P_NONE},
	[0xe5] = {"set 4,l", P_NONE},
	[0xe6] = {"set 4,(hl)", P_NONE},
	[0xe7] = {"set 4,a", P_NONE},
	[0xe8] = {"set 5,b", P_NONE},
	[0xe9] = {"set 5,c", P_NONE},
	[0xea] = {"set 5,d", P_NONE},
	[0xeb] = {"set 5,e", P_NONE},
	[0xec] = {"set 5,h", P_NONE},
	[0xed] = {"set 5,l", P_NONE},
	[0xee] = {"set 5,(hl)", P_NONE},
	[0xef] = {"set 5,a", P_NONE},

	[0xf0] = {"set 6,b", P_NONE},
	[0xf1] = {"set 6,c", P_NONE},
	[0xf2] = {"set 6,d", P_NONE},
	[0xf3] = {"set 6,e", P_NONE},
	[0xf4] = {"set 6,h", P_NONE},
	[0xf5] = {"set 6,l", P_NONE},
	[0xf6] = {"set 6,(hl)", P_NONE},
	[0xf7] = {"set 6,a", P_NONE},
	[0xf8] = {"set 7,b", P_NONE},
	[0xf9] = {"set 7,c", P_NONE},
	[0xfa] = {"set 7,d", P_NONE},
	[0xfb] = {"set 7,e", P_NONE},
	[0xfc] = {"set 7,h", P_NONE},
	[0xfd] = {"set 7,l", P_NONE},
	[0xfe] = {"set 7,(hl)", P_NONE},
	[0xff] = {"set 7,a", P_NONE},
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

static struct line *next_ins(struct line *prev_line)
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
		u8 *db = &gl_stream[pc];

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

	ins = gl_stream[pc];

	format_line(line, HEX_DEFAULT, ins);
	pc++;

	if (ins == 0xC9) // ret
		line->flags |= FORCE_RETURN_CARRIAGE;
	else if (ins == 0xCB) { // extended set
		ins_set = extended_ins_set;
		ins = gl_stream[pc++];
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
		sprintf(&linebuff[13], "%02X %02X", gl_stream[pc], gl_stream[pc + 1]);
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
		u8 p = gl_stream[pc++];
		sprintf(&linebuff[13], "%02X", p);
		linebuff[15] = ' ';
		sprintf(&linebuff[27], label, p);
		break;
	}

	case P_CHAR8:
	{
		int8_t p = gl_stream[pc];
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

PyObject *disasm(PyObject *self, PyObject *args)
{
	int bank_id = 0;
	struct line *line;

	PyArg_ParseTuple(args, "i", &bank_id);
	pc = bank_id * 0x4000;
	lines = line = next_ins(NULL);
	while (pc < (bank_id + 1) * 0x4000)
		line = next_ins(line);
	return get_buffer();
}
