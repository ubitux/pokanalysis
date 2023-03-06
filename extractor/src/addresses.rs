use crate::{reader::Addr, structures::RomStructureTable};

pub const MAP_HEADER_BANKS: Addr = Addr::new(0x03, 0x423d);
pub const MAP_HEADER_POINTERS: Addr = Addr::new(0x00, 0x01ae);
pub const MAP_TILESETS: Addr = Addr::new(0x03, 0x47be);
pub const MAP_ENTITY_DECALS: Addr = Addr::new(0x05, 0x7b27);

pub const MAP_HIDDENS: Addr = Addr::new(0x11, 0x6a40);
pub const MAP_HIDDEN_ITEMS_SCRIPT: Addr = Addr::new(0x1d, 0x6688);
pub const MAP_HIDDEN_COINS_SCRIPT: Addr = Addr::new(0x1d, 0x6799);

pub const MAP_WILD_PKMN: Addr = Addr::new(0x03, 0x4eeb);
pub const WILD_PKMN_PROBABILITIES: Addr = Addr::new(0x04, 0x7918);

pub const PKMN_ORDER: Addr = Addr::new(0x10, 0x5024); // Pokédex order
pub const PKMN_HEADERS: Addr = Addr::new(0x0e, 0x43de); // All 150 first Gen 1 Pokémons
pub const PKMN_MEW_HEADER: Addr = Addr::new(0x01, 0x425b); // Mew is located elsewhere ¯\_(ツ)_/¯
pub const PKMN_TYPES: Addr = Addr::new(0x09, 0x7dae);
pub const PKMN_DETAILS: Addr = Addr::new(0x10, 0x447e); // Detailed information
pub const PKMN_EVENTS: Addr = Addr::new(0x0e, 0x705c); // Evolutions and moves learnset

pub const TEXT_TMHM_NAMES: Addr = Addr::new(0x04, 0x7773);

pub const TRAINER_CLASS: Addr = Addr::new(0x0e, 0x5d3b);
pub const TRAINER_HEADER: Addr = Addr::new(0x0e, 0x5914);

pub enum NamePointer {
    Pokemons,
    Attacks,
    // Badges (unused?)
    Items,
    // Pokémon player team names? (ram location)
    // Pokémon enemy team names? (ram location)
    Trainers,
}

pub fn get_name_pointer(stream: &Vec<u8>, id: NamePointer) -> Addr {
    // Weird table of pointers, doesn't even contain the bank
    const TEXT_NAME_POINTERS: Addr = Addr::new(0x00, 0x375d);
    let (index, bank) = match id {
        NamePointer::Pokemons => (0, 0x07),
        NamePointer::Attacks => (1, 0x2c),
        NamePointer::Items => (3, 0x01),
        NamePointer::Trainers => (6, 0x0e),
    };
    let table = RomStructureTable::<u16>::new_at(stream, TEXT_NAME_POINTERS);
    let ptr = table.entry_at(index);
    Addr::new(bank, ptr)
}

pub fn get_pic_bank_from_rom_pkmn_id(pkmn_id: u8) -> u8 {
    if pkmn_id == 0x15 {
        0x01 // Mew
    } else if pkmn_id == 0xb6 {
        0x0b // Fossil Kabutops
    } else if pkmn_id < 0x1f {
        0x09
    } else if pkmn_id < 0x4a {
        0x0a
    } else if pkmn_id < 0x74 {
        0x0b
    } else if pkmn_id < 0x99 {
        0x0c
    } else {
        0x0d
    }
}
