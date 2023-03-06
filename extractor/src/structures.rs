use crate::reader::{Addr, Reader};

use std::marker::PhantomData;

pub trait RomStructure {
    fn read(reader: &mut Reader) -> Self;
    fn size() -> usize;
}

pub struct RomStructureTable<'a, T> {
    stream: &'a Vec<u8>,
    addr: Addr,
    index: usize,
    _data_type: PhantomData<T>, // XXX comment this bs
}

impl<'a, T: RomStructure> RomStructureTable<'a, T> {
    pub fn new_at(stream: &'a Vec<u8>, addr: Addr) -> Self {
        Self {
            stream,
            addr,
            index: 0,
            _data_type: PhantomData,
        }
    }

    pub fn entry_at(&self, index: u8) -> T {
        let mut reader = Reader::new_at(self.stream, self.addr);
        reader.skip((index as usize * T::size()) as u16);
        T::read(&mut reader)
    }
}

impl<'a, T: RomStructure> Iterator for RomStructureTable<'a, T> {
    type Item = T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index == 0xff {
            None
        } else {
            let ret = self.entry_at(self.index as u8);
            self.index += 1;
            Some(ret)
        }
    }
}

impl RomStructure for u8 {
    fn read(reader: &mut Reader) -> u8 {
        reader.read_u8()
    }

    fn size() -> usize {
        1
    }
}

impl RomStructure for u16 {
    fn read(reader: &mut Reader) -> u16 {
        reader.read_u16()
    }

    fn size() -> usize {
        2
    }
}

pub struct WildPokemon {
    pub level: u8,
    pub id: u8,
}

impl RomStructure for WildPokemon {
    fn read(reader: &mut Reader) -> Self {
        Self {
            level: reader.read_u8(),
            id: reader.read_u8(),
        }
    }

    fn size() -> usize {
        2
    }
}

pub struct WildProbability {
    pub proba: u8,
    pub slot_num_x2: u8, // index × 2, pretty much useless
}

impl RomStructure for WildProbability {
    fn read(reader: &mut Reader) -> Self {
        Self {
            proba: reader.read_u8(),
            slot_num_x2: reader.read_u8(),
        }
    }

    fn size() -> usize {
        2
    }
}

pub struct TrainerHeader {
    pub pic_addr: u16,
    pub money: [u8; 3],
}

impl RomStructure for TrainerHeader {
    fn read(reader: &mut Reader) -> Self {
        Self {
            pic_addr: reader.read_u16(),
            money: reader.read_u8s(),
        }
    }

    fn size() -> usize {
        5
    }
}

pub struct PokemonHeader {
    pub id: u8,
    pub hp: u8,
    pub atk: u8,
    pub def: u8,
    pub spd: u8,
    pub spe: u8,
    pub types: [u8; 2],
    pub capture_rate: u8,
    pub base_exp_yield: u8,
    pub sprite_front_dim: u8,
    pub sprite_front_addr: u16,
    pub sprite_back_addr: u16,
    pub initial_atk: [u8; 4],
    pub growth_rate: u8,
    pub tmhm_flags: [u8; 8],
}

impl RomStructure for PokemonHeader {
    fn read(reader: &mut Reader) -> Self {
        Self {
            id: reader.read_u8(),
            hp: reader.read_u8(),
            atk: reader.read_u8(),
            def: reader.read_u8(),
            spd: reader.read_u8(),
            spe: reader.read_u8(),
            types: reader.read_u8s(),
            capture_rate: reader.read_u8(),
            base_exp_yield: reader.read_u8(),
            sprite_front_dim: reader.read_u8(),
            sprite_front_addr: reader.read_u16(),
            sprite_back_addr: reader.read_u16(),
            initial_atk: reader.read_u8s(),
            growth_rate: reader.read_u8(),
            tmhm_flags: reader.read_u8s(),
        }
    }

    fn size() -> usize {
        28
    }
}

#[derive(Default)]
pub struct MapHeader {
    pub tileset_id: u8,
    pub h: u8,
    pub w: u8,
    pub map_ptr: u16, // "blocks"
    pub text_ptr: u16,
    pub script_ptr: u16,
    pub connect_byte: u8, // for map stitching (overworld)
}

impl RomStructure for MapHeader {
    fn read(reader: &mut Reader) -> Self {
        Self {
            tileset_id: reader.read_u8(),
            h: reader.read_u8(),
            w: reader.read_u8(),
            map_ptr: reader.read_u16(),
            text_ptr: reader.read_u16(),
            script_ptr: reader.read_u16(),
            connect_byte: reader.read_u8(),
        }
    }

    fn size() -> usize {
        10
    }
}

impl MapHeader {
    pub fn n_cons(&self) -> u8 {
        let cb = self.connect_byte;
        (cb >> 3 & 1) + (cb >> 2 & 1) + (cb >> 1 & 1) + (cb & 1)
    }
}

pub struct MapConnection {
    pub map_id: u8,
    pub blocks_src: u16,
    pub blocks_dst: u16,
    pub length: u8,
    pub width: u8,
    pub y_align: u8,
    pub x_align: u8,
    pub window: u16,
}

impl RomStructure for MapConnection {
    fn read(reader: &mut Reader) -> Self {
        Self {
            map_id: reader.read_u8(),
            blocks_src: reader.read_u16(),
            blocks_dst: reader.read_u16(),
            length: reader.read_u8(),
            width: reader.read_u8(),
            y_align: reader.read_u8(),
            x_align: reader.read_u8(),
            window: reader.read_u16(),
        }
    }

    fn size() -> usize {
        11
    }
}

pub struct MapWarp {
    pub y: u8,
    pub x: u8,
    pub to_warp: u8,
    pub to_map: u8,
}

impl RomStructure for MapWarp {
    fn read(reader: &mut Reader) -> Self {
        Self {
            y: reader.read_u8(),
            x: reader.read_u8(),
            to_warp: reader.read_u8(),
            to_map: reader.read_u8(),
        }
    }

    fn size() -> usize {
        4
    }
}

pub struct MapSign {
    pub y: u8,
    pub x: u8,
    pub text_id: u8,
}

impl RomStructure for MapSign {
    fn read(reader: &mut Reader) -> Self {
        Self {
            y: reader.read_u8(),
            x: reader.read_u8(),
            text_id: reader.read_u8(),
        }
    }

    fn size() -> usize {
        3
    }
}

pub struct MapEntity {
    pub pic_id: u8,
    pub y: u8,
    pub x: u8,
    pub movement: u8,    // walking (0xfe) or immobile (0xff)
    pub orientation: u8, // walking pattern or looking direction
    pub text_id: u8,     // text ID but also contain an entity "type"
    pub extra_id: u8,    // item ID (item only) or trainer ID (trainer only)
    pub extra_num: u8,   // trainer number (trainer only)
}

impl RomStructure for MapEntity {
    fn read(reader: &mut Reader) -> Self {
        let pic_id = reader.read_u8();
        let y = reader.read_u8();
        let x = reader.read_u8();
        let movement = reader.read_u8();
        let orientation = reader.read_u8();
        let text_id = reader.read_u8();
        let extra_id = if (text_id & (1 << 7 | 1 << 6)) != 0 {
            reader.read_u8()
        } else {
            0
        };
        let extra_num = if (text_id & (1 << 6)) != 0 {
            reader.read_u8()
        } else {
            0
        };
        Self {
            pic_id,
            y,
            x,
            movement,
            orientation,
            text_id,
            extra_id,
            extra_num,
        }
    }

    fn size() -> usize {
        panic!("size is variable depending on the content");
    }
}

pub struct Tileset {
    pub bank: u8,
    pub blocks_addr: u16,
    pub tiles_addr: u16,
    pub collisions_addr: u16,
    pub counter_tile0: u8,
    pub counter_tile1: u8,
    pub counter_tile2: u8,
    pub grass_tile: u8,
    pub animation_tile: u8,
}

impl RomStructure for Tileset {
    fn read(reader: &mut Reader) -> Self {
        Self {
            bank: reader.read_u8(),
            blocks_addr: reader.read_u16(),
            tiles_addr: reader.read_u16(),
            collisions_addr: reader.read_u16(),
            counter_tile0: reader.read_u8(),
            counter_tile1: reader.read_u8(),
            counter_tile2: reader.read_u8(),
            grass_tile: reader.read_u8(),
            animation_tile: reader.read_u8(),
        }
    }

    fn size() -> usize {
        12
    }
}

pub struct EntityDecal {
    pub addr: u16,
    pub nb_tiles: u8, // probably for the animation as it contains 1 or 3 sprites (nb_sprite x 16 x 2x2)
    pub bank: u8,
}

impl RomStructure for EntityDecal {
    fn read(reader: &mut Reader) -> Self {
        Self {
            addr: reader.read_u16(),
            nb_tiles: reader.read_u8(),
            bank: reader.read_u8(),
        }
    }

    fn size() -> usize {
        4
    }
}

pub struct HiddenEntry {
    pub y: u8,
    pub x: u8,
    pub id: u8, // item or text id
    pub bank: u8,
    pub addr: u16,
}

impl RomStructure for HiddenEntry {
    fn read(reader: &mut Reader) -> Self {
        Self {
            y: reader.read_u8(),
            x: reader.read_u8(),
            id: reader.read_u8(),
            bank: reader.read_u8(),
            addr: reader.read_u16(),
        }
    }

    fn size() -> usize {
        6
    }
}
