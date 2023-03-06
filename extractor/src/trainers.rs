use std::{error::Error, fs, path::Path};

use serde::Serialize;

use crate::{
    addresses::{get_name_pointer, NamePointer, TRAINER_CLASS, TRAINER_HEADER},
    image::Image2bpp,
    pokedex::Pokedex,
    reader::{Addr, Reader},
    sprites::Decoder,
    structures::{RomStructureTable, TrainerHeader},
    text::load_packed_text_id,
};

#[derive(Serialize)]
pub struct TrainerPokemonMember {
    level: u8,
    dex_id: u8,
}

impl TrainerPokemonMember {
    fn new(stream: &Vec<u8>, level: u8, mon_id: u8) -> Self {
        let dex_id = Pokedex::rom_id_to_dex_id(stream, mon_id);
        Self { level, dex_id }
    }
}

#[derive(Serialize)]
pub struct TrainerPokemonTeam(Vec<TrainerPokemonMember>);

impl TrainerPokemonTeam {
    pub fn new(stream: &Vec<u8>, class_id: u8, set_id: u8) -> Self {
        let parties_table = RomStructureTable::<u16>::new_at(stream, TRAINER_CLASS);
        let parties_addr = Addr::new(TRAINER_CLASS.bank, parties_table.entry_at(class_id));
        let mut reader = Reader::new_at(stream, parties_addr);

        // Locate trainer pkmn set (there are multiple parties for a
        // given trainer class)
        for _ in 1..set_id {
            // Each set has a variable number of entries nul terminated
            while reader.read_u8() != 0 {}
        }

        // Read the data set
        let level = reader.read_u8();
        let raw_data: Vec<_> = (0..)
            .map(|_| reader.read_u8())
            .take_while(|v| *v != 0)
            .collect();

        // Interpret the data set as pokemon ids or or pair of pokemon ids and level
        let team = if level != 0xff {
            raw_data
                .iter()
                .map(|mon| TrainerPokemonMember::new(stream, level, *mon))
                .collect()
        } else {
            raw_data
                .chunks_exact(2)
                .map(|v| TrainerPokemonMember::new(stream, v[0], v[1]))
                .collect()
        };

        Self(team)
    }
}

#[derive(Serialize)]
pub struct TrainerClassInfo {
    name: String,
    sprite_path: String,
    base_money: u8, // base_money × last Pokémon level
}

impl TrainerClassInfo {
    fn load(stream: &Vec<u8>, header: &TrainerHeader, id: u8) -> Self {
        let text_addr = get_name_pointer(stream, NamePointer::Trainers);
        let name = load_packed_text_id(stream, text_addr, id + 1);

        // The money seems to be stored in a 3 bytes long BCD but
        // in practice only one byte is in use so we take a shortcut.
        let money = header.money;
        assert_eq!(money[0], 0); // Cents are always 0: $XX.00
        assert_eq!(money[2], 0); // There is nothing above $99 base money
        let base_money = (money[1] >> 4) * 10 + (money[1] & 0xf);

        let sprite_path = format!("trainers/trainer-{:02x}-{}.png", id, name);

        Self {
            name,
            sprite_path,
            base_money,
        }
    }
}

pub struct TrainerClass {
    info: TrainerClassInfo,
    pic: Image2bpp,
}

impl TrainerClass {
    pub fn new(stream: &Vec<u8>, header: &TrainerHeader, id: u8) -> Self {
        let info = TrainerClassInfo::load(stream, &header, id);

        let mut decoder = Decoder::new(stream, Addr::new(0x13, header.pic_addr));
        let pic = decoder.load_sprite();

        Self { info, pic }
    }

    fn export_picture(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        self.pic.save_to_png(&outdir.join(&self.info.sprite_path))?;
        Ok(())
    }
}

pub struct Trainers(Vec<TrainerClass>);

impl Trainers {
    pub fn load(stream: &Vec<u8>, nb_trainers: usize) -> Self {
        let table = RomStructureTable::<TrainerHeader>::new_at(stream, TRAINER_HEADER);
        assert!(nb_trainers < 0xff);
        let trainers = table
            .take(nb_trainers)
            .enumerate()
            .map(|(id, header)| TrainerClass::new(&stream, &header, id as u8))
            .collect();
        Self(trainers)
    }

    pub fn export_pictures(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        fs::create_dir_all(outdir.join("trainers"))?;
        for trainer in self.0.iter() {
            trainer.export_picture(outdir)?
        }
        Ok(())
    }

    pub fn export_info(&self) -> Vec<&TrainerClassInfo> {
        self.0.iter().map(|trainer| &trainer.info).collect()
    }
}
