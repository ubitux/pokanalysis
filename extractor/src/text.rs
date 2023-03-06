use std::iter;

use crate::{
    addresses::{get_name_pointer, NamePointer},
    reader::{Addr, Reader},
};

fn get_pkmn_chr<'a>(c: u8) -> &'a str {
    match c {
        0x49 => "\n", // pokédex desc sentence separator
        0x4E => "\n", // pokédex desc normal \n
        0x4F => "\n",
        0x51 => "\n\n",
        0x52 => "<PlayerName>",
        0x53 => "<RivalName>",
        0x54 => "POKÉ",
        0x55 => "\n", // stop with arrow indicator
        0x7F => " ",
        0x80 => "A",
        0x81 => "B",
        0x82 => "C",
        0x83 => "D",
        0x84 => "E",
        0x85 => "F",
        0x86 => "G",
        0x87 => "H",
        0x88 => "I",
        0x89 => "J",
        0x8A => "K",
        0x8B => "L",
        0x8C => "M",
        0x8D => "N",
        0x8E => "O",
        0x8F => "P",
        0x90 => "Q",
        0x91 => "R",
        0x92 => "S",
        0x93 => "T",
        0x94 => "U",
        0x95 => "V",
        0x96 => "W",
        0x97 => "X",
        0x98 => "Y",
        0x99 => "Z",
        0x9A => "(",
        0x9B => ")",
        0x9C => ":",
        0x9D => ";",
        0x9E => "]",
        0x9F => "[",
        0xA0 => "a",
        0xA1 => "b",
        0xA2 => "c",
        0xA3 => "d",
        0xA4 => "e",
        0xA5 => "f",
        0xA6 => "g",
        0xA7 => "h",
        0xA8 => "i",
        0xA9 => "j",
        0xAA => "k",
        0xAB => "l",
        0xAC => "m",
        0xAD => "n",
        0xAE => "o",
        0xAF => "p",
        0xB0 => "q",
        0xB1 => "r",
        0xB2 => "s",
        0xB3 => "t",
        0xB4 => "u",
        0xB5 => "v",
        0xB6 => "w",
        0xB7 => "x",
        0xB8 => "y",
        0xB9 => "z",
        0xBA => "É",
        0xBB => "'d",
        0xBC => "'l",
        0xBD => "'s",
        0xBE => "'t",
        0xBF => "'v",
        0xE0 => "'",
        0xE1 => "PK",
        0xE2 => "MN",
        0xE3 => "-",
        0xE4 => "'r",
        0xE5 => "'m",
        0xE6 => "?",
        0xE7 => "!",
        0xE8 => ".",
        0xEF => "♂",
        0xF0 => "$",
        0xF1 => "x",
        0xF2 => ".",
        0xF3 => "/",
        0xF4 => ",",
        0xF5 => "♀",
        0xF6 => "0",
        0xF7 => "1",
        0xF8 => "2",
        0xF9 => "3",
        0xFA => "4",
        0xFB => "5",
        0xFC => "6",
        0xFD => "7",
        0xFE => "8",
        0xFF => "9",
        _ => {
            panic!("unknown char 0x{:02x}", c);
        }
    }
}

pub fn get_text_command(reader: &mut Reader) -> String {
    let cmd = reader.read_u8();
    match cmd {
        // 0x01 => "<Text from ram>".into(),
        0x08 => "<Script>".into(),
        0x17 => {
            let addr = reader.read_u16();
            let bank = reader.read_u8();

            reader.seek(Addr::new(bank, addr));
            let op = reader.read_u8();
            if op != 0x00 {
                panic!("unknown starting text code 0x{:02x}", op);
            }
            get_text(reader)
        }
        0xf5 => "<Script: Vending machine>".into(),
        0xf6 => "<Script: Cable club>".into(),
        0xf7 => "<Script: Prize vendor>".into(),
        0xfe => "<Script: Mart>".into(),
        0xff => "<Script: Nurse>".into(),
        _ => {
            panic!("unknown command 0x{:02x}", cmd);
        }
    }
}

pub fn get_text(reader: &mut Reader) -> String {
    iter::repeat(0)
        .map(|_| reader.read_u8())
        // 0x50 when reading pokédex species (generic stop)
        // 0x57 when reading signs
        // 0x5f when reading pokédex description
        .take_while(|c| ![0x57, 0x50, 0x5f].contains(c))
        .map(|c| get_pkmn_chr(c))
        .collect()
}

pub fn decode_text_fixed(s: &[u8], n: usize) -> String {
    s[0..n]
        .iter()
        .take_while(|c| **c != 0x50)
        .map(|c| get_pkmn_chr(*c))
        .collect()
}

// Reader is focused on the start of a list of variable length text string
// separated by 0x50. The list itself is not terminated and has no labels for
// the entries, so we have to crawl through the whole list every time, just
// like the game.
pub fn load_packed_text_id(stream: &Vec<u8>, addr: Addr, id: u8) -> String {
    let mut reader = Reader::new_at(stream, addr);
    assert_ne!(id, 0);
    for _ in 0..id - 1 {
        get_text(&mut reader); // XXX only stopped by 0x50 here
    }
    get_text(&mut reader)
}

// XXX preload?
pub fn get_item_name(stream: &Vec<u8>, id: u8) -> String {
    if id > 250 {
        format!("HM{:02}", id - 250)
    } else if id > 200 {
        format!("TM{:02}", id - 200)
    } else {
        let addr = get_name_pointer(stream, NamePointer::Items);
        load_packed_text_id(stream, addr, id)
    }
}
