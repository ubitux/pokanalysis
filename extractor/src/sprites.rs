use crate::image::{Image2bpp, TILE_PIXELS_1D};
use crate::reader::{Addr, Reader};
use crate::tiling::tiles_to_7x7_colmajor;

pub const PLANE7_SIZE: usize = 7 * 7 * 8; // 1bpp plane composed of 7x7 tiles (8x8 pixels each)
pub const SPRITE7_SIZE: usize = PLANE7_SIZE * 2; // 2bpp sprite composed of 7x7 tiles (8x8 pixels each)
pub const SPRITE7_LINESIZE: usize = 7 * 2;

struct BitReader<'a> {
    reader: Reader<'a>,
    byte: u8,
    bit: u8,
}

impl<'a> BitReader<'a> {
    fn new(stream: &'a Vec<u8>, addr: Addr) -> Self {
        Self {
            reader: Reader::new_at(stream, addr),
            byte: 0,
            bit: 0,
        }
    }
}

impl<'a> Iterator for BitReader<'a> {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        // XXX need a stop event, maybe on read_u8 error or EOF?
        if self.bit == 0 {
            self.byte = self.reader.read_u8();
            self.bit = 8;
        }
        self.bit -= 1;
        Some(self.byte >> self.bit & 1)
    }
}

struct DibitPacker {
    byte_pos: usize,
    dibit_pos: u8,
    size: usize,
    storage: [u8; PLANE7_SIZE],
}

impl DibitPacker {
    fn new(size: usize) -> Self {
        Self {
            byte_pos: 0,
            dibit_pos: 0,
            size,
            storage: [0u8; PLANE7_SIZE],
        }
    }

    fn push_zero_dibits(&mut self, count: u16) {
        // Since the buffer is initialized with zeros we can make a direct jump
        // to the position instead of calling push_dibit() count times
        let bytes_skip = count / 4;
        let bit_skip = count % 4;
        self.byte_pos += bytes_skip as usize;
        self.dibit_pos += bit_skip as u8;
        if self.dibit_pos > 3 {
            self.dibit_pos -= 4;
            self.byte_pos += 1;
        }
        assert!(self.byte_pos <= self.size);
    }

    fn push_dibit(&mut self, dibit: u8) {
        assert!(!self.is_full());
        self.storage[self.byte_pos] |= dibit << ((3 - self.dibit_pos) * 2);
        self.dibit_pos = if self.dibit_pos == 3 {
            self.byte_pos += 1;
            0
        } else {
            self.dibit_pos + 1
        };
    }

    fn is_full(&self) -> bool {
        self.byte_pos == self.size
    }
}

pub struct Decoder<'a> {
    br: BitReader<'a>,
    sprite_w: usize,
    sprite_h: usize,
    linesize_1bpp: usize,
    size_1bpp: usize,
}

impl<'a> Decoder<'a> {
    const CODE_MAP: [[u8; 16]; 2] = [
        [0, 1, 3, 2, 7, 6, 4, 5, 15, 14, 12, 13, 8, 9, 11, 10],
        [15, 14, 12, 13, 8, 9, 11, 10, 0, 1, 3, 2, 7, 6, 4, 5],
    ];

    pub fn new(stream: &'a Vec<u8>, addr: Addr) -> Self {
        Self {
            br: BitReader::new(stream, addr),
            sprite_w: 0,
            sprite_h: 0,
            linesize_1bpp: 0,
            size_1bpp: 0,
        }
    }

    fn get_code(i: u8, prev: u8) -> u8 {
        Self::CODE_MAP[(prev & 1) as usize][i as usize]
    }

    fn delta_decode(&self, data: &mut [u8]) {
        for x in 0..self.sprite_w {
            let mut b = 0;
            // XXX remove division
            // XXX can we find the bitwise or dibitwise operation?
            for y in 0..self.sprite_h / 8 {
                let idx = y * self.sprite_w + x;
                let dy = data[idx];
                let a = Self::get_code(dy >> 4, b);
                b = Self::get_code(dy & 0xf, a);
                data[idx] = a << 4 | b;
            }
        }
    }

    fn xor(dst: &mut [u8], src: &[u8], len: usize) {
        for k in 0..len {
            dst[k] ^= src[k];
        }
    }

    fn decode(&mut self, plane0: &mut [u8], plane1: &mut [u8], encoding: u8) {
        match encoding {
            0 => {
                // XXX untested codepath
                self.delta_decode(plane0);
                self.delta_decode(plane1);
            }
            1 => {
                self.delta_decode(plane0);
                Self::xor(plane1, &plane0, self.size_1bpp);
            }
            2 => {
                self.delta_decode(plane0);
                self.delta_decode(plane1);
                Self::xor(plane1, &plane0, self.size_1bpp);
            }
            _ => {
                panic!("packing");
            }
        }
    }

    // Run-length decoding of zero dibits
    fn read_rle(&mut self, bp: &mut DibitPacker) {
        let br = &mut self.br;
        let nb_ones = br.take_while(|bit| *bit == 1).count();
        assert!(nb_ones < 15); // Starting 15 we can have overflows in `count`
        let ones = (2 << nb_ones) - 1;
        let value = br
            .take(nb_ones + 1)
            .fold(0u16, |acc, bit| acc << 1 | (bit as u16));
        let count = ones + value;
        bp.push_zero_dibits(count)
    }

    // Raw decoding of non-zero dibits
    fn read_raw(&mut self, bp: &mut DibitPacker) {
        loop {
            let dibit = self.br.next().unwrap() << 1 | self.br.next().unwrap();
            if dibit == 0 {
                break;
            }
            bp.push_dibit(dibit);
            if bp.is_full() {
                break;
            }
        }
    }

    fn read_plane(&mut self, bp: &mut DibitPacker) -> [u8; PLANE7_SIZE] {
        let mut read_mode = self.br.next().unwrap();
        while !bp.is_full() {
            if read_mode == 0 {
                self.read_rle(bp);
            } else {
                self.read_raw(bp);
            }
            read_mode ^= 1;
        }

        self.transpose_dibit_plane(&bp)
    }

    // XXX simplify this shit
    // XXX move to the packer? (rename the struct to DibitPlane?)
    fn transpose_dibit_plane(&self, bp: &DibitPacker) -> [u8; PLANE7_SIZE] {
        let mut out_bp = DibitPacker::new(bp.size);

        for y in (0..self.sprite_h / 2).step_by(4) {
            for x in 0..self.sprite_w {
                for obit_pos in 0..4 {
                    let dibit_pos = (y + obit_pos) * self.sprite_w + x;
                    let byte_pos = dibit_pos / 4;
                    let ibit_pos = dibit_pos % 4;
                    let v = bp.storage[byte_pos] >> ((3 - ibit_pos) * 2) & 0b11;
                    out_bp.push_dibit(v)
                }
            }
        }
        out_bp.storage
    }

    fn read_encoding(&mut self) -> u8 {
        if self.br.next().unwrap() == 1 {
            self.br.next().unwrap() + 1
        } else {
            0
        }
    }

    fn merge_planes(
        &self,
        plane0: &[u8; PLANE7_SIZE],
        plane1: &[u8; PLANE7_SIZE],
    ) -> [u8; SPRITE7_SIZE] {
        let mut dst = [0u8; SPRITE7_SIZE];
        for idx in 0..self.size_1bpp {
            // These 2 write operations correspond to 1 tile linesize (8 pixels) in 2bpp
            dst[idx * 2 + 0] = plane0[idx];
            dst[idx * 2 + 1] = plane1[idx];
        }
        dst
    }

    fn extend_canvas(&self, src: &[u8; SPRITE7_SIZE]) -> [u8; SPRITE7_SIZE] {
        let mut dst = [0u8; SPRITE7_SIZE];
        let w = self.sprite_w / 8;
        let h = self.sprite_h / 8;

        let mut i = 0;
        let align = 8 * (7 * ((10 - w) / 2) - h);
        for x in 0..w {
            for y in 0..self.sprite_h {
                let pos = align + x * 7 * 8 + y;
                dst[pos * 2] = src[i];
                dst[pos * 2 + 1] = src[i + 1];
                i += 2;
            }
        }

        dst
    }

    pub fn load_sprite(&mut self) -> Image2bpp {
        let dim = self.br.reader.read_u8();

        // XXX w and h should be swapped: during decode it's correct,
        // but it doesn't correspond to the final output
        // renaming will allow the use of of "linesize" for sprite_h
        let w = (dim >> 4) as usize;
        let h = (dim & 0xf) as usize;
        self.sprite_w = 8 * w; // XXX rename to linesize and remove the 8?
        self.sprite_h = 8 * h;
        self.linesize_1bpp = w;
        self.size_1bpp = w * h * 8;

        // size units: bytes
        let mut bp0 = DibitPacker::new(self.size_1bpp);
        let mut bp1 = DibitPacker::new(self.size_1bpp);

        // Read planes and their encoding
        let primary = self.br.next().unwrap();
        let mut plane0 = self.read_plane(&mut bp0);
        let encoding = self.read_encoding();
        let mut plane1 = self.read_plane(&mut bp1);

        // Decode planes according to the encoding
        self.decode(&mut plane0, &mut plane1, encoding);

        // Combine the two 1bpp planes into one 2bpp sprite
        let sprite = if primary == 0 {
            self.merge_planes(&plane0, &plane1)
        } else {
            self.merge_planes(&plane1, &plane0)
        };

        // Extend the sprite into a 7x7 frame
        let sprite = self.extend_canvas(&sprite);

        let ret = tiles_to_7x7_colmajor(&sprite);
        Image2bpp::from_data(7 * TILE_PIXELS_1D, 7 * TILE_PIXELS_1D, ret.to_vec())
    }
}
