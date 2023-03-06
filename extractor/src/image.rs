use std::error::Error;
use std::fs;
use std::io::BufWriter;
use std::path::PathBuf;

use png;
use serde::Serialize;

pub const SPRITE_TILE_H: usize = 2; // Sprites are 2×2 tiles
pub const BLOCK_TILE_H: usize = 4; // Blocks are 4×4 tiles (or 2×2 sprites)

// Number of pixels in 1D (row and columns are identical)
pub const TILE_PIXELS_1D: usize = 8; // Tiles are 8x8 pixels
pub const SPRITE_PIXELS_1D: usize = SPRITE_TILE_H * TILE_PIXELS_1D;
pub const BLOCK_PIXELS_1D: usize = BLOCK_TILE_H * TILE_PIXELS_1D;

// Line sizes are in bytes
pub const TILE_LINESIZE: usize = TILE_PIXELS_1D * 2 / 8;
pub const SPRITE_LINESIZE: usize = TILE_LINESIZE * 2;
pub const BLOCK_LINESIZE: usize = TILE_LINESIZE * 4;

// Total size in bytes
pub const TILE_SIZE: usize = TILE_LINESIZE * TILE_PIXELS_1D;
pub const SPRITE_SIZE: usize = SPRITE_LINESIZE * SPRITE_PIXELS_1D;
pub const BLOCK_SIZE: usize = BLOCK_LINESIZE * BLOCK_PIXELS_1D;

#[derive(Clone, Copy)]
pub enum ElementType {
    Default,
    Warp,
    Sign,
    Entity,
    Hidden,
}

#[derive(Clone, Copy, Serialize)]
pub struct SpritePosition(pub u8, pub u8);

pub struct Marker {
    pub pos: SpritePosition,
    category: ElementType,
    sprite: Option<Image2bpp>,
}

impl Marker {
    pub fn new(pos: SpritePosition, category: ElementType, sprite: Option<Image2bpp>) -> Self {
        Self {
            pos,
            category,
            sprite,
        }
    }
}

pub struct Image2bpp {
    pub width: usize,
    pub height: usize,
    pub linesize: usize,
    pub data: Vec<u8>,
}

impl Image2bpp {
    pub fn new(width: usize, height: usize) -> Self {
        assert_eq!(width % 4, 0);
        let linesize = width / 4;
        let data = vec![0u8; linesize * height];
        Self {
            width,
            height,
            data,
            linesize,
        }
    }

    pub fn from_data(width: usize, height: usize, data: Vec<u8>) -> Self {
        assert_eq!(width % 4, 0);
        assert_eq!(width * height / 4, data.len());
        Self {
            width,
            height,
            data,
            linesize: width / 4,
        }
    }

    pub fn save_to_png(&self, path: &PathBuf) -> Result<(), Box<dyn Error>> {
        let pic_rgb = Image24bpp::from_2bpp(self);
        pic_rgb.save_to_png(path)?;
        Ok(())
    }
}

#[derive(Debug)]
pub struct Image24bpp {
    pub width: usize,
    pub height: usize,
    pub linesize: usize,
    pub data: Vec<u8>,
}

impl Image24bpp {
    const BPP: usize = 3; // RGB

    pub fn new(width: usize, height: usize) -> Self {
        let data = vec![0u8; width * height * Self::BPP];
        Self {
            width,
            height,
            data,
            linesize: width * Self::BPP,
        }
    }

    pub fn from_2bpp(img2bpp: &Image2bpp) -> Self {
        let width = img2bpp.width;
        let height = img2bpp.height;
        let mut data = vec![0u8; width * height * Self::BPP];
        let palette = Self::get_element_palette(ElementType::Default);

        // convert 2 bytes per iteration, corresponding to 8 pixels
        let src_iter = img2bpp.data.chunks_exact(TILE_LINESIZE);
        let dst_iter = data.chunks_exact_mut(TILE_PIXELS_1D * Self::BPP);
        for (src, dst) in src_iter.zip(dst_iter) {
            Self::write_rgb_tile_row(dst, src, &palette);
        }

        Self {
            width,
            height,
            data,
            linesize: img2bpp.width * Self::BPP,
        }
    }

    const fn get_element_palette(element_type: ElementType) -> [[u8; 3]; 4] {
        match element_type {
            ElementType::Default => [
                [0xE8, 0xE8, 0xE8], // Lightest
                [0x58, 0x58, 0x58], // Light tint
                [0xA0, 0xA0, 0xA0], // Dark tint
                [0x10, 0x10, 0x10], // Darkest
            ],
            ElementType::Warp => [
                [0xE8, 0xC0, 0xC0],
                [0xC0, 0x58, 0x58],
                [0xC0, 0xA0, 0xA0],
                [0xC0, 0x10, 0x10],
            ],
            ElementType::Sign => [
                [0xC0, 0xC0, 0xE8],
                [0x58, 0x58, 0xC0],
                [0xA0, 0xA0, 0xC0],
                [0x10, 0x10, 0xC0],
            ],
            ElementType::Entity => [
                [0xFF, 0xFF, 0xFF], // Unused: reserved for transparency
                [0xE0, 0x58, 0xE8], // Color tint
                [0xDA, 0xC0, 0xC0], // Light color, skin
                [0x58, 0x10, 0x58], // Black, outline
            ],
            ElementType::Hidden => [
                [0xFF, 0xFF, 0xFF], // Unused: reserved for transparency
                [0xFF, 0x40, 0x00], // Hidden item
                [0x40, 0xFF, 0x00], // Hidden other
                [0xFF, 0xFF, 0xFF], // Unused
            ],
        }
    }

    pub fn blend(&mut self, src: &Image24bpp, px: usize, py: usize) {
        for row in 0..src.height {
            let dst_pos = (py + row) * self.linesize + px * Self::BPP;
            let src_pos = row * src.linesize;
            self.data[dst_pos..dst_pos + src.linesize]
                .copy_from_slice(&src.data[src_pos..src_pos + src.linesize]);
        }
    }

    pub fn apply_markers(&mut self, markers: Vec<Marker>, img2bpp: &Image2bpp) {
        let width = self.width / BLOCK_PIXELS_1D;
        let height = self.height / BLOCK_PIXELS_1D;

        let src_linesize = img2bpp.linesize;
        let dst_linesize = self.linesize;

        for marker in markers {
            let palette = Self::get_element_palette(marker.category);

            let sx = marker.pos.0 as usize;
            let sy = marker.pos.1 as usize;

            if sx >= width * 2 || sy >= height * 2 {
                eprintln!("unexpected out of bound marker located at {},{}", sx, sy);
                continue;
            }

            let dst_sprite_pos =
                dst_linesize * SPRITE_PIXELS_1D * sy + SPRITE_PIXELS_1D * Self::BPP * sx;

            match marker.sprite {
                // Replace color
                None => {
                    let src_sprite_pos =
                        src_linesize * SPRITE_PIXELS_1D * sy + SPRITE_LINESIZE * sx;

                    for row in 0..SPRITE_PIXELS_1D {
                        let src_pos = src_sprite_pos + src_linesize * row;
                        let dst_pos = dst_sprite_pos + dst_linesize * row;
                        Self::write_rgb_tile_row(
                            &mut self.data[dst_pos..],
                            &img2bpp.data[src_pos..src_pos + TILE_LINESIZE],
                            &palette,
                        );
                        Self::write_rgb_tile_row(
                            &mut self.data[dst_pos + 8 * Self::BPP..],
                            &img2bpp.data[src_pos + TILE_LINESIZE..src_pos + 2 * TILE_LINESIZE],
                            &palette,
                        );
                    }
                }
                // Overlay the sprite
                Some(sprite) => {
                    assert_eq!(sprite.width, SPRITE_PIXELS_1D);
                    assert_eq!(sprite.height, SPRITE_PIXELS_1D);
                    for row in 0..SPRITE_PIXELS_1D {
                        let src_pos = SPRITE_LINESIZE * row;
                        let dst_pos = dst_sprite_pos + dst_linesize * row;
                        Self::write_rgb_tile_row_honor_alpha(
                            &mut self.data[dst_pos..],
                            &sprite.data[src_pos..src_pos + TILE_LINESIZE],
                            &palette,
                        );
                        Self::write_rgb_tile_row_honor_alpha(
                            &mut self.data[dst_pos + 8 * Self::BPP..],
                            &sprite.data[src_pos + TILE_LINESIZE..src_pos + 2 * TILE_LINESIZE],
                            &palette,
                        );
                    }
                }
            }
        }
    }

    fn write_rgb_tile_row(dst: &mut [u8], row: &[u8], palette: &[[u8; 3]; 4]) {
        let shr_iter = (0..8).rev();
        let dst_iter = dst.chunks_exact_mut(Self::BPP);
        for (shift, dst_chunk) in shr_iter.zip(dst_iter) {
            let px = (row[0] >> shift & 1) << 1 | (row[1] >> shift & 1);
            let color = palette[px as usize];
            dst_chunk.copy_from_slice(&color);
        }
    }

    fn write_rgb_tile_row_honor_alpha(dst: &mut [u8], row: &[u8], palette: &[[u8; 3]; 4]) {
        let shr_iter = (0..8).rev();
        let dst_iter = dst.chunks_exact_mut(Self::BPP);
        for (shift, dst_chunk) in shr_iter.zip(dst_iter) {
            let px = (row[0] >> shift & 1) << 1 | (row[1] >> shift & 1);
            if px == 0 {
                continue;
            }
            let color = palette[px as usize];
            dst_chunk.copy_from_slice(&color);
        }
    }

    pub fn save_to_png(&self, path: &PathBuf) -> Result<(), Box<dyn Error>> {
        println!("saving {:?}", path);

        let file = fs::File::create(path)?;
        let ref mut w = BufWriter::new(file);

        let width = self.width as u32;
        let height = self.height as u32;

        let mut encoder = png::Encoder::new(w, width, height);
        encoder.set_color(png::ColorType::Rgb);
        encoder.set_depth(png::BitDepth::Eight);
        let mut writer = encoder.write_header()?;
        writer.write_image_data(&self.data)?;
        Ok(())
    }
}
