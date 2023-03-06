use std::collections::HashMap;

use serde::Serialize;

use crate::{
    addresses::WILD_PKMN_PROBABILITIES,
    pokedex::Pokedex,
    reader::{Addr, Reader},
    structures::{RomStructure, RomStructureTable, WildPokemon, WildProbability},
};

struct Probabilities(Vec<u8>);

// TODO store in a shared context somewhere to avoid re-reading them all the time
impl Probabilities {
    fn load(stream: &Vec<u8>) -> Self {
        let table = RomStructureTable::<WildProbability>::new_at(stream, WILD_PKMN_PROBABILITIES);

        // Build the wild Pokémon probabilities for each of the 10 slots
        let probas: Vec<_> = [0u8]
            .into_iter()
            .chain(table.take(10).map(|v| v.proba))
            .collect::<Vec<_>>()
            .windows(2)
            .map(|s| s[1] - s[0])
            .collect();

        Self(probas)
    }
}

#[derive(Serialize)]
struct WildPokemonInfo {
    dex_id: u8,
    level: u8,
    proba: u8,
}

impl WildPokemonInfo {
    fn new(raw: &WildPokemon, proba: u8, stream: &Vec<u8>) -> Self {
        Self {
            dex_id: Pokedex::rom_id_to_dex_id(stream, raw.id),
            level: raw.level,
            proba,
        }
    }
}

#[derive(Serialize)]
struct WildPokemonLocationInfo {
    rate: u8,
    pokemons: Vec<WildPokemonInfo>,
}

impl WildPokemonLocationInfo {
    fn new(reader: &mut Reader) -> Self {
        let stream = reader.stream;
        let rate = reader.read_u8();

        // Read the 10 slots and associate them with their corresponding probability
        let pokemons = if rate != 0 {
            let probas = Probabilities::load(stream);
            (0..10)
                .map(|_| WildPokemon::read(reader))
                .zip(probas.0.into_iter())
                .map(|(pkmn, proba)| WildPokemonInfo::new(&pkmn, proba, stream))
                .collect()
        } else {
            Vec::new()
        };

        // Merge the probabilities: since there are always 10 entries, some may
        // be redundant. We merge them when pokemon and level are identical.
        let mut hm = HashMap::new();
        pokemons.iter().for_each(|pkmn| {
            hm.entry((pkmn.dex_id, pkmn.level))
                .and_modify(|proba| *proba += pkmn.proba)
                .or_insert(pkmn.proba);
        });
        let mut pokemons: Vec<_> = hm
            .iter()
            .map(|(k, v)| WildPokemonInfo {
                dex_id: k.0,
                level: k.1,
                proba: *v,
            })
            .collect();

        // Sort the final list according to the new merged probabilities. Reverse
        // in order to have the highest probabilities first.
        pokemons.sort_by_key(|p| std::cmp::Reverse((p.proba, p.dex_id, p.level)));

        // Make sure the probabilities add up to 100%
        if pokemons.len() != 0 {
            let total: u8 = pokemons.iter().map(|v| v.proba).sum();
            assert_eq!(total, 0xff);
        }

        Self { rate, pokemons }
    }
}

#[derive(Serialize)]
pub struct WildPokemonsInfo {
    grass: Option<WildPokemonLocationInfo>,
    water: Option<WildPokemonLocationInfo>,
}

impl WildPokemonsInfo {
    pub fn new(addr: Addr, stream: &Vec<u8>) -> Self {
        let mut reader = Reader::new_at(stream, addr);
        let grass_pkmn = WildPokemonLocationInfo::new(&mut reader);
        let water_pkmn = WildPokemonLocationInfo::new(&mut reader);
        Self {
            grass: if grass_pkmn.rate != 0 {
                Some(grass_pkmn)
            } else {
                None
            },
            water: if water_pkmn.rate != 0 {
                Some(water_pkmn)
            } else {
                None
            },
        }
    }
}
