use std::{collections::HashMap, iter};

use serde::Serialize;

use crate::{
    addresses::{MAP_HIDDENS, MAP_HIDDEN_COINS_SCRIPT, MAP_HIDDEN_ITEMS_SCRIPT},
    image::{Image2bpp, SpritePosition, SPRITE_PIXELS_1D, SPRITE_SIZE},
    reader::{Addr, Reader},
    structures::HiddenEntry,
    text::get_item_name,
};

#[derive(Serialize)]
pub struct HiddenInfo {
    pub pos: SpritePosition,
    pub content: Option<String>,
}

impl HiddenInfo {
    pub fn new(raw: &HiddenEntry, stream: &Vec<u8>) -> Self {
        let pos = SpritePosition(raw.x, raw.y);
        let addr = Addr::new(raw.bank, raw.addr);
        let content = if addr == MAP_HIDDEN_ITEMS_SCRIPT {
            Some(get_item_name(stream, raw.id))
        } else if addr == MAP_HIDDEN_COINS_SCRIPT {
            Some(format!("{} coins", raw.id))
        } else {
            None
        };
        Self { pos, content }
    }

    // Build a frame to be blend onto the 2x2 tile; the palette_id
    // identifies the border color
    pub fn get_sprite(&self, palette_id: u8) -> Image2bpp {
        assert_eq!(palette_id & 0b11, palette_id);
        assert_ne!(palette_id, 0); // transparent color
        let b0 = palette_id >> 1;
        let b1 = palette_id & 1;
        let filled_tile = [b0 * 0xff, b1 * 0xff]; // top + bottom
        let left_tile = [b0 << 7, b1 << 7];
        let right_tile = [b0, b1];
        let mut sprite: Vec<u8> = vec![0; SPRITE_SIZE];
        sprite[0..2].copy_from_slice(&filled_tile);
        sprite[2..4].copy_from_slice(&filled_tile);
        for i in 1..SPRITE_PIXELS_1D - 1 {
            sprite[i * 4..i * 4 + 2].copy_from_slice(&left_tile);
            sprite[i * 4 + 2..i * 4 + 4].copy_from_slice(&right_tile);
        }
        sprite[60..62].copy_from_slice(&filled_tile);
        sprite[62..64].copy_from_slice(&filled_tile);

        Image2bpp::from_data(SPRITE_PIXELS_1D, SPRITE_PIXELS_1D, sprite)
    }
}

pub struct HiddensIndex(pub HashMap<u8, Addr>);

impl HiddensIndex {
    pub fn new(stream: &Vec<u8>) -> Self {
        // Read the full list of map IDs
        let mut reader = Reader::new_at(stream, MAP_HIDDENS);
        let map_ids: Vec<_> = iter::repeat(0)
            .map(|_| reader.read_u8())
            .take_while(|map_id| *map_id != 0xff)
            .collect();
        if map_ids.len() > 254 {
            panic!("hidden object table is too large: {}", map_ids.len());
        }

        // The list of addresses follows directly the list of map IDs
        let hm = map_ids
            .into_iter()
            .map(|map_id| (map_id as u8, Addr::new(MAP_HIDDENS.bank, reader.read_u16())))
            .collect();

        Self(hm)
    }
}
