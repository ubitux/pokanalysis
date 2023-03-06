use std::env;
use std::error::Error;
use std::fs;
use std::path::Path;

mod addresses;
mod entities;
mod hiddens;
mod image;
mod maps;
mod pokedex;
mod reader;
mod sprites;
mod structures;
mod text;
mod tiling;
mod trainers;
mod wild;

use maps::{MapInfo, Maps, OverworldInfo};
use pokedex::{Pokedex, PokemonInfo};
use trainers::{TrainerClassInfo, Trainers};

use serde::Serialize;

struct Rom {
    pokedex: Pokedex,
    maps: Maps,
    trainers: Trainers,
}

#[derive(Serialize)]
struct Export<'a> {
    pokedex: Vec<&'a PokemonInfo>,
    maps: Vec<Option<&'a MapInfo>>,
    overworld: &'a OverworldInfo,
    trainers: Vec<&'a TrainerClassInfo>,
}

impl Rom {
    fn from_path(file_path: impl AsRef<Path>) -> Result<Self, std::io::Error> {
        // The ROM files are small (1M), so we can just load them entirely in memory
        let stream = fs::read(file_path)?;

        // Parse everything
        let pokedex = Pokedex::load(&stream);
        let maps = Maps::load(&stream);
        let trainers = Trainers::load(&stream, maps.get_trainers_count());

        Ok(Self {
            pokedex,
            maps,
            trainers,
        })
    }

    fn export_data(&self, outdir: impl AsRef<Path>) -> Result<(), Box<dyn Error>> {
        let outdir = outdir.as_ref();

        // Export all pictures
        self.pokedex.export_pictures(outdir)?;
        self.maps.export_pictures(outdir)?;
        self.trainers.export_pictures(outdir)?;

        // Serialize the data in a JSON file
        let export = Export {
            pokedex: self.pokedex.export_info(),
            maps: self.maps.export_maps_info(),
            overworld: &self.maps.overworld_info,
            trainers: self.trainers.export_info(),
        };
        let output = serde_json::to_string_pretty(&export)?;
        fs::write(&outdir.join("data.json"), &output)?;

        Ok(())
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() != 3 {
        Err(format!("usage: {} <rom.gb> <outdir>", &args[0]).into())
    } else {
        let file_path = &args[1];
        let outdir = &args[2];
        Rom::from_path(file_path)?.export_data(outdir)?;
        Ok(())
    }
}
