use crate::{
    image::{
        BLOCK_LINESIZE, BLOCK_PIXELS_1D, BLOCK_SIZE, SPRITE_LINESIZE, SPRITE_PIXELS_1D,
        SPRITE_SIZE, TILE_LINESIZE, TILE_PIXELS_1D,
    },
    sprites::{SPRITE7_LINESIZE, SPRITE7_SIZE},
};

/// Lay out a stream of 2D patches (`src)` onto a 2D surface (`dst`).
///
/// The reflow operates in a row major order: source patches are layed out
/// horizontally until they reach the end of the line, then goes onto the next
/// one.
///
/// `dst_linesize` and `src_linesize` corresponds to the widths expressed in bytes.
/// `dst_rows` and `src_rows` corresponds to the number of rows.
///
/// Example:
/// ```
/// A
/// B   ->   AB
/// C   ->   CD
/// D
/// ```
pub fn reflow(
    dst: &mut [u8],
    dst_linesize: usize,
    dst_rows: usize,
    src: &[u8],
    src_linesize: usize,
    src_rows: usize,
) {
    // XXX this function would be much better if rust supported generic const
    // expression such as fn<const N: usize>(a: &[N*N])
    let src_iter = src.chunks_exact(src_linesize);
    let row_area = src_rows * dst_linesize;
    let dst_iter = (0..dst_rows * dst_linesize)
        .step_by(row_area)
        // src_linestart: buffer position of the top-left of each patch of the left-most column
        .flat_map(move |src_linestart| {
            (src_linestart..src_linestart + dst_linesize)
                .step_by(src_linesize)
                // src_start: buffer position of the top-left of all patches
                .flat_map(move |src_start| (src_start..src_start + row_area).step_by(dst_linesize))
        });

    for (src, dst_start) in src_iter.zip(dst_iter) {
        dst[dst_start..dst_start + src_linesize].copy_from_slice(src);
    }
}

/// Same as `reflow` but lay out the 2D patches in column major order (vertical
/// stripes from left to right)
/// Example:
/// ```
/// A
/// B   ->   AC
/// C   ->   BD
/// D
/// ```
pub fn reflow_colmajor(
    dst: &mut [u8],
    dst_linesize: usize,
    dst_rows: usize,
    src: &[u8],
    src_linesize: usize,
) {
    let src_iter = src.chunks_exact(src_linesize);

    // Minimal buffer size that contain a given column of patches
    // (we obviously need to skip a lot of space)
    let tiles_col_area = (dst_rows - 1) * dst_linesize + src_linesize;

    let dst_iter = (0..dst_linesize)
        .step_by(src_linesize)
        // src_colstart: buffer position of the top-left of each patch of the top-most row
        .flat_map(move |src_colstart| {
            (src_colstart..src_colstart + tiles_col_area)
                .step_by(dst_linesize)
                // src_start: buffer position of the top-left of all patches
                .flat_map(move |src_start| {
                    (src_start..src_start + src_linesize).step_by(dst_linesize)
                })
        });

    for (src, dst_start) in src_iter.zip(dst_iter) {
        dst[dst_start..dst_start + src_linesize].copy_from_slice(src);
    }
}

// Tiles stream to 7x7, layed out top to bottom first
pub fn tiles_to_7x7_colmajor(tiles: &[u8; SPRITE7_SIZE]) -> [u8; SPRITE7_SIZE] {
    let mut sprite7 = [0u8; SPRITE7_SIZE];
    reflow_colmajor(&mut sprite7, SPRITE7_LINESIZE, 7 * 8, tiles, TILE_LINESIZE);
    sprite7
}

// Tiles stream to Sprite
pub fn tiles_to_2x2_rowmajor(tiles: &[u8; SPRITE_SIZE]) -> [u8; SPRITE_SIZE] {
    let mut sprite = [0u8; SPRITE_SIZE];
    reflow(
        &mut sprite,
        SPRITE_LINESIZE,
        SPRITE_PIXELS_1D,
        tiles,
        TILE_LINESIZE,
        TILE_PIXELS_1D,
    );
    sprite
}

// Tiles stream to Block
pub fn tiles_to_4x4_rowmajor(tiles: &[u8]) -> [u8; BLOCK_SIZE] {
    let mut block = [0u8; BLOCK_SIZE];
    reflow(
        &mut block,
        BLOCK_LINESIZE,
        BLOCK_PIXELS_1D,
        tiles,
        TILE_LINESIZE,
        TILE_PIXELS_1D,
    );
    block
}

pub fn blocks_to_map(dst: &mut [u8], blocks: &Vec<u8>, w: usize, h: usize) {
    let map_linesize = w * BLOCK_LINESIZE;
    let map_rows = h * BLOCK_PIXELS_1D;
    reflow(
        dst,
        map_linesize,
        map_rows,
        blocks,
        BLOCK_LINESIZE,
        BLOCK_PIXELS_1D,
    );
}
