use std::{error::Error, fs, path::Path};

use serde::Serialize;

use crate::{
    addresses::{
        get_name_pointer, get_pic_bank_from_rom_pkmn_id, NamePointer, PKMN_DETAILS, PKMN_EVENTS,
        PKMN_HEADERS, PKMN_MEW_HEADER, PKMN_ORDER, PKMN_TYPES, TEXT_TMHM_NAMES,
    },
    image::Image2bpp,
    reader::{Addr, BlobSlicer, Reader},
    sprites::Decoder,
    structures::{PokemonHeader, RomStructure, RomStructureTable},
    text::{decode_text_fixed, get_item_name, get_text, get_text_command, load_packed_text_id},
};

#[derive(Serialize)]
enum Evolution {
    Level { pkmn_id: u8, level: u8 },
    Stone { pkmn_id: u8, stone: String },
    Exchange { pkmn_id: u8 },
}

#[derive(Serialize)]
pub struct PokemonInfo {
    name: String,
    species_name: String,
    types: Vec<String>,
    height: String,
    weight: String,
    desc: String,
    hp: u8,
    atk: u8,
    def: u8,
    spd: u8,
    spe: u8,
    cap: u8,
    exp: u8,
    attacks: Vec<(u8, String)>,
    growth_rate: String,
    evolutions: Vec<Evolution>,
    tmhm: Vec<(String, String)>,
    sprite_front_path: String,
    sprite_back_path: String,
}

impl PokemonInfo {
    fn load(stream: &Vec<u8>, header: &PokemonHeader) -> Self {
        let id = header.id;
        let rom_id = Pokedex::dex_id_to_rom_id(stream, id);
        assert_ne!(rom_id, 0);

        // Dedicated table with all the names in fixed length buffers
        // (10 bytes, non-terminated)
        assert_ne!(rom_id, 0);
        let pkmn_names_addr = get_name_pointer(stream, NamePointer::Pokemons);
        let slicer = BlobSlicer::<10>::new_at(stream, pkmn_names_addr);
        let name = slicer.slice_at(rom_id - 1);
        let name = decode_text_fixed(name, 10);

        // List of pointers to detailed information for each Pokémon
        let table = RomStructureTable::<u16>::new_at(stream, PKMN_DETAILS);
        let dex_entry_addr = Addr::new(PKMN_DETAILS.bank, table.entry_at(rom_id - 1));

        // Read detailed information; each entry is of variable size
        // depending on the initial string length (the species name)
        let mut reader = Reader::new_at(stream, dex_entry_addr);
        let species_name = get_text(&mut reader);
        let height_feet = reader.read_u8();
        let height_inches = reader.read_u8();
        let weight_pounds = reader.read_u16();
        let height = format!("{}'{:02}\"", height_feet, height_inches);
        let weight = format!("{}.{}lb", weight_pounds / 10, weight_pounds % 10);
        let desc = get_text_command(&mut reader);

        // Types
        let table = RomStructureTable::<u16>::new_at(stream, PKMN_TYPES);
        let text_addr = Addr::new(PKMN_TYPES.bank, table.entry_at(header.types[0]));
        let mut reader = Reader::new_at(stream, text_addr);
        let mut types = vec![get_text(&mut reader)];
        if header.types[1] != header.types[0] {
            let text_addr = Addr::new(PKMN_TYPES.bank, table.entry_at(header.types[1]));
            let mut reader = Reader::new_at(stream, text_addr);
            types.push(get_text(&mut reader));
        }

        // Attack moves
        let atk_moves_addr = get_name_pointer(stream, NamePointer::Attacks);
        let mut attacks: Vec<_> = header
            .initial_atk
            .iter()
            .take_while(|id| **id != 0)
            .map(|id| (0, load_packed_text_id(stream, atk_moves_addr, *id)))
            .collect();

        // Events part 1: evolutions
        let mut evolutions = Vec::new();
        let table = RomStructureTable::<u16>::new_at(stream, PKMN_EVENTS);
        let events_addr = Addr::new(PKMN_EVENTS.bank, table.entry_at(rom_id - 1));
        let mut reader = Reader::new_at(stream, events_addr);
        loop {
            let evo_type = reader.read_u8();
            if evo_type == 0 {
                break;
            }
            let evo = match evo_type {
                1 => Evolution::Level {
                    level: reader.read_u8(),
                    pkmn_id: Pokedex::rom_id_to_dex_id(stream, reader.read_u8()),
                },
                2 => {
                    let item_id = reader.read_u8();
                    let level = reader.read_u8();
                    assert_eq!(level, 1);
                    Evolution::Stone {
                        stone: get_item_name(stream, item_id),
                        pkmn_id: Pokedex::rom_id_to_dex_id(stream, reader.read_u8()),
                    }
                }
                3 => {
                    let level = reader.read_u8();
                    assert_eq!(level, 1);
                    Evolution::Exchange {
                        pkmn_id: Pokedex::rom_id_to_dex_id(stream, reader.read_u8()),
                    }
                }
                _ => {
                    panic!("unknown evolution 0x{:02x}", evo_type);
                }
            };
            evolutions.push(evo);
        }

        // Events part 2: attack moves learnset
        loop {
            let level = reader.read_u8();
            if level == 0 {
                break;
            }
            let atk = reader.read_u8();
            let atk_name = load_packed_text_id(stream, atk_moves_addr, atk);
            attacks.push((level, atk_name));
        }

        // Growth rate
        let growth_rate = match header.growth_rate {
            0 => "Medium Fast".into(),
            3 => "Medium Slow".into(),
            4 => "Fast".into(),
            5 => "Slow".into(),
            _ => {
                panic!("unknown growth rate 0x{:02x}", header.growth_rate);
            }
        };

        // TM/HM learnset: combine the 8 bytes of TM/HM flags into a single 64-
        // bit long bitfield then check the 50+5 available values
        let tmhm_mask: u64 = header
            .tmhm_flags
            .iter()
            .enumerate()
            .map(|(i, byte)| (*byte as u64) << (8 * i))
            .fold(0, |acc, e| acc | e);
        let mut tmhm = Vec::new();
        let table = RomStructureTable::<u8>::new_at(stream, TEXT_TMHM_NAMES);
        for i in 201..=255 {
            let id = i - 201;
            let mask = (1 as u64) << id;
            if (mask & tmhm_mask) != 0 {
                let move_id = table.entry_at(id);
                let item_name = get_item_name(stream, i as u8);
                let move_name = load_packed_text_id(stream, atk_moves_addr, move_id);
                tmhm.push((move_name, item_name));
            }
        }

        // Sprite destination
        let sprite_front_path = format!("pkmn/pkmn-front-{:03}-{}.png", id, name);
        let sprite_back_path = format!("pkmn/pkmn-back-{:03}-{}.png", id, name);

        Self {
            name,
            species_name,
            types,
            height,
            weight,
            desc,
            hp: header.hp,
            atk: header.atk,
            def: header.def,
            spd: header.spd,
            spe: header.spe,
            cap: header.capture_rate,
            exp: header.base_exp_yield,
            attacks,
            growth_rate,
            evolutions,
            tmhm,
            sprite_front_path,
            sprite_back_path,
        }
    }
}

pub struct Pokemon {
    info: PokemonInfo,
    pic_front: Image2bpp,
    pic_back: Image2bpp,
}

impl Pokemon {
    pub fn new(stream: &Vec<u8>, header: &PokemonHeader) -> Self {
        let info = PokemonInfo::load(stream, header);

        let id = header.id;
        let rom_id = Pokedex::dex_id_to_rom_id(stream, id);
        assert_ne!(rom_id, 0);

        // let dim = header.sprite_front_dim;
        let bank = get_pic_bank_from_rom_pkmn_id(rom_id);
        let mut dec_front = Decoder::new(stream, Addr::new(bank, header.sprite_front_addr));
        let mut dec_back = Decoder::new(stream, Addr::new(bank, header.sprite_back_addr));
        let pic_front = dec_front.load_sprite();
        let pic_back = dec_back.load_sprite();

        Self {
            info,
            pic_front,
            pic_back,
        }
    }

    fn export_picture(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        self.pic_front
            .save_to_png(&outdir.join(&self.info.sprite_front_path))?;
        self.pic_back
            .save_to_png(&outdir.join(&self.info.sprite_back_path))?;
        Ok(())
    }
}

pub struct Pokedex(Vec<Pokemon>);

impl Pokedex {
    pub fn load(stream: &Vec<u8>) -> Self {
        let mut pokedex = Vec::new();

        let table = RomStructureTable::<PokemonHeader>::new_at(stream, PKMN_HEADERS);
        for i in 0..150 {
            let header = table.entry_at(i);
            assert_eq!(header.id, i + 1); // list is ordered
            pokedex.push(Pokemon::new(stream, &header));
        }

        let mut mew_reader = Reader::new_at(stream, PKMN_MEW_HEADER);
        let mew_header = PokemonHeader::read(&mut mew_reader);
        assert_eq!(mew_header.id, 151);
        pokedex.push(Pokemon::new(stream, &mew_header));

        Self(pokedex)
    }

    pub fn export_pictures(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        fs::create_dir_all(outdir.join("pkmn"))?;
        for pkmn in self.0.iter() {
            pkmn.export_picture(outdir)?;
        }
        Ok(())
    }

    pub fn export_info(&self) -> Vec<&PokemonInfo> {
        self.0.iter().map(|pkmn| &pkmn.info).collect()
    }

    pub fn rom_id_to_dex_id(stream: &Vec<u8>, rom_id: u8) -> u8 {
        assert_ne!(rom_id, 0);
        let table = RomStructureTable::<u8>::new_at(stream, PKMN_ORDER);
        table.entry_at(rom_id - 1)
    }

    pub fn dex_id_to_rom_id(stream: &Vec<u8>, dex_id: u8) -> u8 {
        // This retarded loop is what the game is actually doing
        let mut reader = Reader::new_at(stream, PKMN_ORDER);
        // A sane mind would start at 0, but the game stores them in the ROM as
        // index+1 (typically in the evolutions), so we're consistent with it
        for rom_id in 1..255 {
            let pokedex_id = reader.read_u8();
            if pokedex_id == dex_id {
                return rom_id;
            }
        }
        panic!("pokemon id {} not found", dex_id);
    }
}
