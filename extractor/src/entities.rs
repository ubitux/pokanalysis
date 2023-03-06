use serde::Serialize;

use crate::{
    addresses::MAP_ENTITY_DECALS,
    image::{Image2bpp, SpritePosition, SPRITE_LINESIZE, SPRITE_PIXELS_1D, SPRITE_SIZE},
    pokedex::Pokedex,
    reader::{Addr, BlobSlicer, Reader},
    structures::{EntityDecal, MapEntity, RomStructureTable},
    text::{get_item_name, get_text_command},
    tiling::tiles_to_2x2_rowmajor,
    trainers::TrainerPokemonTeam,
};

#[derive(Clone, Copy)]
enum DecalOrientation {
    Default, // This ignores DecalBehaviour
    Down,
    Up,
    Left,
    Right,
}

enum DecalBehaviour {
    Still,
    Walking,
}

#[derive(Serialize)]
pub enum EntityData {
    NormalPeople {
        text: String,
    },
    Trainer {
        class_id: u8,
        team: TrainerPokemonTeam,
        text: Option<String>,
    },
    Pokemon {
        dex_id: u8,
        level: u8,
    },
    Item {
        name: String,
        text: Option<String>,
    },
}

#[derive(Serialize)]
pub struct EntityInfo {
    pub pos: SpritePosition,
    pub data: EntityData,
}

impl EntityInfo {
    pub fn new(raw: &MapEntity, stream: &Vec<u8>, text_addr: Addr) -> Self {
        let text_id = raw.text_id & 0b00111111;
        let table = RomStructureTable::<u16>::new_at(stream, text_addr);
        let addr = Addr::new(text_addr.bank, table.entry_at(text_id - 1));
        let mut reader = Reader::new_at(stream, addr);
        let text = get_text_command(&mut reader);

        assert!(raw.x >= 4 && raw.y >= 4);
        let pos = SpritePosition(raw.x - 4, raw.y - 4);
        let data = if (raw.text_id & (1 << 7)) != 0 {
            assert_eq!(raw.text_id & (1 << 6), 0);
            if raw.extra_id != 0 {
                assert_eq!(text, "<Script>");
                let name = get_item_name(stream, raw.extra_id);
                EntityData::Item { name, text: None }
            } else {
                // There are 2 items with a script (in Blue house)
                assert_ne!(text, "<Script>");
                eprintln!("detected unknown item");
                EntityData::Item {
                    name: String::from("unknown"),
                    text: Some(text),
                }
            }
        } else if (raw.text_id & (1 << 6)) != 0 {
            assert_eq!(raw.text_id & (1 << 7), 0);

            if raw.extra_id >= 0xc9 {
                // There is 1 trainer with a text (Giovanni)
                let text = if text == "<Script>" { None } else { Some(text) };
                // TODO we need to do something better about the <Script>, but
                // unfortunately it's not consistent in the ROM. There are 3
                // texts associated with each trainer, and they are handled as
                // dedicated script systematically; also, the bytecode sequence
                // is not always consistent meaning parsing the text may
                // involve ASM tracing. For now, the texts are considered None
                // (except in the case of Giovanni).
                // XXX is this different in the Japanese versions and the other
                // languages?
                let class_id = raw.extra_id - 0xc9;
                let set_id = raw.extra_num;
                let team = TrainerPokemonTeam::new(stream, class_id, set_id);
                EntityData::Trainer {
                    text,
                    class_id,
                    team,
                }
            } else {
                assert_eq!(text, "<Script>");
                EntityData::Pokemon {
                    dex_id: Pokedex::rom_id_to_dex_id(stream, raw.extra_id),
                    level: raw.extra_num,
                }
            }
        } else {
            EntityData::NormalPeople { text }
        };
        Self { pos, data }
    }
}

impl MapEntity {
    pub fn get_sprite(&self, stream: &Vec<u8>, rng: &mut u32) -> Image2bpp {
        let decal_behaviour = match self.movement {
            0xfe => DecalBehaviour::Walking,
            0xff => DecalBehaviour::Still,
            _ => {
                panic!("unrecognized movement behaviour 0x{:02x}", self.movement);
            }
        };

        let decal_orient = match self.orientation {
            0x00 => Self::random_choice::<DecalOrientation>(
                rng,
                vec![
                    DecalOrientation::Down,
                    DecalOrientation::Up,
                    DecalOrientation::Left,
                    DecalOrientation::Right,
                ],
            ),
            0x01 => Self::random_choice::<DecalOrientation>(
                rng,
                vec![DecalOrientation::Down, DecalOrientation::Up],
            ),
            0x02 => Self::random_choice::<DecalOrientation>(
                rng,
                vec![DecalOrientation::Left, DecalOrientation::Right],
            ),
            0x10 => DecalOrientation::Default, // boulder
            0xd0 => DecalOrientation::Down,
            0xd1 => DecalOrientation::Up,
            0xd2 => DecalOrientation::Left,
            0xd3 => DecalOrientation::Right,
            0xff => DecalOrientation::Default, // no direction
            _ => {
                panic!("unrecognized entity orientation {}", self.orientation);
            }
        };

        let table = RomStructureTable::<EntityDecal>::new_at(stream, MAP_ENTITY_DECALS);
        assert_ne!(self.pic_id, 0);
        let decal = table.entry_at(self.pic_id - 1);

        let decals_addr = Addr::new(decal.bank, decal.addr);
        // up to 6 slices of decals (2x2 tiles each -> 64)
        let decals_slicer = BlobSlicer::<64>::new_at(stream, decals_addr);

        // Decals:
        // 0: static look down
        // 1: static look up
        // 2: static look left
        // 3: walk down
        // 4: walk up
        // 5: walk left
        let decal_offset = match decal_behaviour {
            DecalBehaviour::Still => 0,
            DecalBehaviour::Walking => 3,
        };
        let decal_id = match decal_orient {
            DecalOrientation::Down => decal_offset + 0,
            DecalOrientation::Up => decal_offset + 1,
            DecalOrientation::Left => decal_offset + 2,
            DecalOrientation::Right => decal_offset + 2, // same as left but h-flipped
            DecalOrientation::Default => {
                assert!(matches!(decal_behaviour, DecalBehaviour::Still));
                0
            }
        };

        let tiles = decals_slicer.slice_at(decal_id);
        let mut sprite = tiles_to_2x2_rowmajor(&tiles);

        if matches!(decal_orient, DecalOrientation::Right) {
            Self::hflip_sprite(&mut sprite);
        }

        Image2bpp::from_data(SPRITE_PIXELS_1D, SPRITE_PIXELS_1D, sprite.to_vec())
    }

    fn random_choice<T>(rng: &mut u32, choices: Vec<T>) -> T
    where
        T: Copy,
    {
        // Basic LCG from Numerical Recipes
        *rng = 1664525u32.wrapping_mul(*rng).wrapping_add(1013904223u32);
        choices[(*rng as usize) % choices.len()]
    }

    fn hflip_sprite(sprite: &mut [u8; SPRITE_SIZE]) {
        sprite.chunks_exact_mut(SPRITE_LINESIZE).for_each(|row| {
            let new_row = [
                row[2].reverse_bits(),
                row[3].reverse_bits(),
                row[0].reverse_bits(),
                row[1].reverse_bits(),
            ];
            row.copy_from_slice(&new_row);
        })
    }
}
