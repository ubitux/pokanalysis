use std::{
    cmp,
    collections::{HashMap, HashSet},
    error::Error,
    fs,
    path::Path,
};

use serde::Serialize;

use crate::{
    addresses::{MAP_HEADER_BANKS, MAP_HEADER_POINTERS, MAP_TILESETS, MAP_WILD_PKMN},
    entities::{EntityData, EntityInfo},
    hiddens::{HiddenInfo, HiddensIndex},
    image::{
        ElementType, Image24bpp, Image2bpp, Marker, SpritePosition, BLOCK_PIXELS_1D,
        SPRITE_PIXELS_1D, TILE_SIZE,
    },
    reader::{Addr, BlobSlicer, Reader},
    structures::{
        HiddenEntry, MapConnection, MapEntity, MapHeader, MapSign, MapWarp, RomStructure,
        RomStructureTable, Tileset,
    },
    text::get_text_command,
    tiling::{blocks_to_map, tiles_to_4x4_rowmajor},
    wild::WildPokemonsInfo,
};

#[derive(Clone, Copy)]
pub enum Orientation {
    E,
    W,
    S,
    N,
}

#[derive(Default, Clone, Copy, Debug, Serialize)]
struct SpriteCoords {
    pub x: i32,
    pub y: i32,
}

impl SpriteCoords {
    pub fn new() -> Self {
        Default::default()
    }

    pub fn offset(&self, orient: Orientation, header: &MapHeader, con: &MapConnection) -> Self {
        match orient {
            Orientation::N => {
                let target_h = con.y_align as i32 + 1;
                let offset = con.x_align as i8 as i32;
                Self {
                    x: self.x - offset,
                    y: self.y - target_h,
                }
            }
            Orientation::S => {
                assert_eq!(con.y_align, 0);
                let offset = con.x_align as i8 as i32;
                Self {
                    x: self.x - offset,
                    y: self.y + header.h as i32 * 2,
                }
            }
            Orientation::W => {
                let target_w = con.x_align as i32 + 1;
                let offset = con.y_align as i8 as i32;
                Self {
                    x: self.x - target_w,
                    y: self.y - offset,
                }
            }
            Orientation::E => {
                assert_eq!(con.x_align, 0);
                let offset = con.y_align as i8 as i32;
                Self {
                    x: self.x + header.w as i32 * 2,
                    y: self.y - offset,
                }
            }
        }
    }
}

#[derive(Serialize)]
struct WarpInfo {
    pos: SpritePosition,
    to_map: u8,
    to_warp: u8,
}

impl WarpInfo {
    fn new(raw: &MapWarp) -> Self {
        Self {
            pos: SpritePosition(raw.x, raw.y),
            to_map: raw.to_map,
            to_warp: raw.to_warp,
        }
    }
}

#[derive(Serialize)]
struct SignInfo {
    pos: SpritePosition,
    text: String,
}

impl SignInfo {
    fn new(raw: &MapSign, stream: &Vec<u8>, text_addr: Addr) -> Self {
        let pos = SpritePosition(raw.x, raw.y);
        let id = raw.text_id;
        assert_ne!(id, 0);
        let table = RomStructureTable::<u16>::new_at(stream, text_addr);
        let addr = Addr::new(text_addr.bank, table.entry_at(id - 1));
        let mut reader = Reader::new_at(stream, addr);
        let text = get_text_command(&mut reader);
        Self { pos, text }
    }
}

#[derive(Serialize)]
pub struct MapInfo {
    warps: Vec<WarpInfo>,
    signs: Vec<SignInfo>,
    entities: Vec<EntityInfo>,
    wild_pkmn: WildPokemonsInfo,
    hiddens: Vec<HiddenInfo>,
    coords: Option<SpriteCoords>, // None if not part of the overworld
    width: u8,
    height: u8,
    pic_path: String,
}

impl MapInfo {
    fn new(smap: &ScannedMap, stream: &Vec<u8>, hiddens_addr: Option<Addr>, id: u8) -> Self {
        let warps = smap.warps.iter().map(|w| WarpInfo::new(w)).collect();

        let text_addr = Addr::new(smap.addr.bank, smap.header.text_ptr);
        let signs = smap
            .signs
            .iter()
            .map(|s| SignInfo::new(s, stream, text_addr))
            .collect();

        let entities = smap
            .entities
            .iter()
            .map(|e| EntityInfo::new(e, stream, text_addr))
            .collect();

        // XXX there is another table of hiddens locations (only for the detector?);
        // XXX check for consistency?
        let hiddens = if let Some(hiddens_addr) = hiddens_addr {
            RomStructureTable::<HiddenEntry>::new_at(stream, hiddens_addr)
                // Note: last struct read is going to overread as bit
                // but that should be harmless
                .take_while(|hidden| hidden.y != 0xff)
                .map(|hidden| HiddenInfo::new(&hidden, stream))
                .collect()
        } else {
            Vec::new()
        };

        // Wild pokémons (grass & water)
        let table = RomStructureTable::<u16>::new_at(stream, MAP_WILD_PKMN);
        let addr = Addr::new(MAP_WILD_PKMN.bank, table.entry_at(id));
        let wild_pkmn = WildPokemonsInfo::new(addr, stream);

        // Coords (overworld only)
        let coords = if smap.connections.len() > 0 {
            Some(smap.coords)
        } else {
            None
        };

        // Picture path and its dimension in block units
        let width = smap.header.w;
        let height = smap.header.h;
        let pic_path = format!("maps/map-{:02x}.png", id);

        Self {
            warps,
            signs,
            entities,
            wild_pkmn,
            hiddens,
            coords,
            width,
            height,
            pic_path,
        }
    }
}

struct Map {
    info: MapInfo,
    pic: Image24bpp,
    trainer_class_ids: Vec<u8>,
}

impl Map {
    fn load(stream: &Vec<u8>, smap: &ScannedMap, id: u8, hiddens_addr: Option<Addr>) -> Self {
        let info = MapInfo::new(smap, stream, hiddens_addr, id);

        let header = &smap.header;
        let width = header.w as usize;
        let height = header.h as usize;

        // Locate tileset to construct the picture
        let tileset_table = RomStructureTable::<Tileset>::new_at(stream, MAP_TILESETS);
        let tileset_id = header.tileset_id;
        let tileset = tileset_table.entry_at(tileset_id);

        // Data readers & Blob slicers
        let datablk_addr = Addr::new(tileset.bank, tileset.blocks_addr);
        let tiles_addr = Addr::new(tileset.bank, tileset.tiles_addr);
        let blkids_addr = Addr::new(smap.addr.bank, header.map_ptr);
        let reader_datablk = BlobSlicer::<16>::new_at(stream, datablk_addr); // 4*4 tiles
        let reader_tiles = BlobSlicer::<TILE_SIZE>::new_at(stream, tiles_addr);
        let mut reader_blkids = Reader::new_at(stream, blkids_addr);

        // Build background picture (the map without the mobile overlays)
        let mut pic_bg = Image2bpp::new(width * BLOCK_PIXELS_1D, height * BLOCK_PIXELS_1D);
        let blocks: Vec<u8> = (0..width * height)
            .flat_map(|_| {
                // Build the 4x4 tiles of the block
                let block_tiles: Vec<u8> = reader_datablk
                    .slice_at(reader_blkids.read_u8())
                    .iter()
                    .flat_map(|tile_id| *reader_tiles.slice_at(*tile_id))
                    .collect();
                // Build the block from the tiles stream
                tiles_to_4x4_rowmajor(&block_tiles)
            })
            .collect();
        blocks_to_map(&mut pic_bg.data, &blocks, width, height);

        // Construct markers: warps, signs, entities, and hiddens
        // XXX add a Into/From to convert these info struct into Markers?
        let mut markers = Vec::new();
        for warp in &info.warps {
            markers.push(Marker::new(warp.pos, ElementType::Warp, None));
        }
        for sign in &info.signs {
            markers.push(Marker::new(sign.pos, ElementType::Sign, None));
        }
        let mut seed = id as u32;
        for (raw, info) in smap.entities.iter().zip(&info.entities) {
            let sprite = raw.get_sprite(stream, &mut seed);
            markers.push(Marker::new(info.pos, ElementType::Entity, Some(sprite)));
        }
        for hidden in &info.hiddens {
            let palette_id = if hidden.content.is_some() { 1 } else { 2 };
            // XXX do not compute it too much?
            let sprite = hidden.get_sprite(palette_id);
            markers.push(Marker::new(hidden.pos, ElementType::Hidden, Some(sprite)))
        }

        // Report conflicting markers
        let mut hm = HashMap::new();
        for marker in &markers {
            let key = (marker.pos.0, marker.pos.1);
            if hm.contains_key(&key) {
                eprintln!(
                    "map 0x{:02x} contains conflicting markers at position ({},{})",
                    id, key.0, key.1
                );
            }
            hm.insert(key, marker);
        }

        // Convert to RGB
        let mut pic = Image24bpp::from_2bpp(&pic_bg);
        pic.apply_markers(markers, &pic_bg);

        // Track the trainer classes in use in this map
        // XXX we could only store the max here
        let trainer_class_ids: Vec<_> = info
            .entities
            .iter()
            .filter_map(|e| match e.data {
                EntityData::Trainer { class_id: id, .. } => Some(id),
                _ => None,
            })
            .collect();

        Map {
            info,
            pic,
            trainer_class_ids,
        }
    }

    fn export_picture(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        self.pic.save_to_png(&outdir.join(&self.info.pic_path))?;
        Ok(())
    }
}

#[derive(Default)]
struct CacheElement<T>(Option<T>);

#[derive(Default)]
struct Cache<T> {
    data: Vec<CacheElement<T>>,
}

impl<T: Default> Cache<T> {
    fn entry(&mut self, key: u8) -> &mut CacheElement<T> {
        let key = key as usize;
        // XXX just hardcode 256?
        if key >= self.data.len() {
            self.data.resize_with(key + 1, Default::default);
        }
        self.data.get_mut(key).unwrap()
    }
}

impl<T> CacheElement<T> {
    fn unwrap(&self) -> &T {
        self.0.as_ref().unwrap()
    }
}

#[derive(Default)]
struct Boundaries {
    min: SpriteCoords,
    max: SpriteCoords,
}

impl Boundaries {
    fn new() -> Self {
        Self {
            min: SpriteCoords {
                x: i32::MAX,
                y: i32::MAX,
            },
            max: SpriteCoords {
                x: i32::MIN,
                y: i32::MIN,
            },
        }
    }

    // Map w & h unit: blocks, pos is in sprite units
    fn stretch(&mut self, pos: SpriteCoords, map_w: u8, map_h: u8) {
        self.min.x = cmp::min(self.min.x, pos.x);
        self.min.y = cmp::min(self.min.y, pos.y);
        self.max.x = cmp::max(self.max.x, pos.x + ((map_w as usize) * 2) as i32);
        self.max.y = cmp::max(self.max.y, pos.y + ((map_h as usize) * 2) as i32);
    }
}

#[derive(Default)]
struct ScannedMap {
    addr: Addr,
    header: MapHeader,
    connections: Vec<MapConnection>,
    warps: Vec<MapWarp>,
    signs: Vec<MapSign>,
    entities: Vec<MapEntity>,
    coords: SpriteCoords,
}

impl ScannedMap {
    fn new(stream: &Vec<u8>, addr: Addr, coords: SpriteCoords) -> Self {
        let mut reader = Reader::new_at(stream, addr);
        let header = MapHeader::read(&mut reader);
        if header.w > 127 || header.h > 127 {
            // This would cause overflows
            panic!(
                "map dimensions are too large {}x{} > (127,127)",
                header.w, header.h
            );
        }

        // Direct connections
        let connections = (0..header.n_cons())
            .map(|_| MapConnection::read(&mut reader))
            .collect();

        // Jump to extended data
        let data_addr = reader.read_u16();
        reader.seek(Addr::new(addr.bank, data_addr));
        reader.skip(1); // map border tile

        // Warps
        let n_warps = reader.read_u8();
        let warps = (0..n_warps).map(|_| MapWarp::read(&mut reader)).collect();

        // Signs
        let n_signs = reader.read_u8();
        let signs = (0..n_signs).map(|_| MapSign::read(&mut reader)).collect();

        // Entities (normal people, trainers, items)
        let n_entities = reader.read_u8();
        let entities = (0..n_entities)
            .map(|_| MapEntity::read(&mut reader))
            .collect();

        Self {
            addr,
            header,
            connections,
            warps,
            signs,
            entities,
            coords,
        }
    }
}

struct MapsScanner<'a> {
    stream: &'a Vec<u8>,
    hiddens: HiddensIndex,
    maps: Cache<ScannedMap>,
}

impl<'a> MapsScanner<'a> {
    // Expected order of orientations for the map connections
    const ORIENTS: [Orientation; 4] = [
        Orientation::N,
        Orientation::S,
        Orientation::W,
        Orientation::E,
    ];

    fn new(stream: &'a Vec<u8>) -> Self {
        Self {
            stream,
            hiddens: HiddensIndex::new(stream),
            maps: Default::default(),
        }
    }

    fn scan(&mut self) -> Vec<Option<Map>> {
        self.recursive_scan(0, SpriteCoords::new());
        self.maps
            .data
            .iter()
            .enumerate()
            .map(|(map_id, xsmap)| {
                assert!(map_id < 0xff);
                match &xsmap.0 {
                    Some(smap) => {
                        let map_id = map_id as u8;
                        let hiddens_addr = self.hiddens.0.get(&map_id).cloned();
                        Some(Map::load(self.stream, &smap, map_id, hiddens_addr))
                    }
                    _ => None,
                }
            })
            .collect()
    }

    fn recursive_scan(&mut self, map_id: u8, pos: SpriteCoords) {
        // 0xed: special elevator exit script?
        // 0xff: means "last overworld map"
        if [0xed, 0xff].contains(&map_id) {
            return;
        }

        // Skip the map if it's already loaded
        // XXX use/make a and_modify()?
        let entry = self.maps.entry(map_id);
        if entry.0.is_some() {
            return;
        }

        // Register map in the cache
        let table_banks = RomStructureTable::<u8>::new_at(self.stream, MAP_HEADER_BANKS);
        let table_addrs = RomStructureTable::<u16>::new_at(self.stream, MAP_HEADER_POINTERS);
        let header_bank = table_banks.entry_at(map_id);
        let header_addr = table_addrs.entry_at(map_id);
        let addr = Addr::new(header_bank, header_addr);
        entry.0.replace(ScannedMap::new(self.stream, addr, pos));
        let map = entry.unwrap();

        // Log the current map being tracked so that messages can be referred back to the map
        if map.header.connect_byte != 0 {
            println!(
                "tracking overworld map 0x{:02x} ({}x{}) at {:?}",
                map_id, map.header.w, map.header.h, pos
            );
        } else {
            println!(
                "tracking map 0x{:02x} ({}x{})",
                map_id, map.header.w, map.header.h
            );
        }

        // Filter enabled orientations. Note that the masks values are in inverted
        // order as declarations
        let orients: Vec<_> = Self::ORIENTS
            .iter()
            .enumerate()
            .filter(|(i, _)| (map.header.connect_byte & (1 << (3 - i))) != 0)
            .map(|(_, o)| o)
            .collect();
        if orients.len() != map.connections.len() {
            panic!("the number of orientation doesn't match number of connections");
        }

        // Identify all links: warps (doors) and direct connection (stitched map of the overworld)
        let warp_links = map.warps.iter().map(|w| (w.to_map, SpriteCoords::new()));
        let cons_links = map
            .connections
            .iter()
            .zip(orients)
            .map(|(c, o)| (c.map_id, pos.offset(*o, &map.header, c)));

        // Recursively scan linked map (either direct connections or warps such as doors)
        warp_links
            .chain(cons_links)
            .collect::<Vec<_>>()
            .into_iter()
            .for_each(|(id, pos)| self.recursive_scan(id, pos));
    }
}

#[derive(Serialize)]
pub struct OverworldInfo {
    // In sprite units (16x16 pixels)
    width: usize,
    height: usize,
    pic_path: String,
}

pub struct Maps {
    maps: Vec<Option<Map>>,
    pub overworld_info: OverworldInfo,
}

impl Maps {
    pub fn load(stream: &Vec<u8>) -> Self {
        let mut scanner = MapsScanner::new(stream);
        let mut maps = scanner.scan();

        // Compute the overworld boundaries and dimensions
        let mut world_bounds = Boundaries::new();
        maps.iter().flatten().for_each(|map| match map.info.coords {
            Some(coords) => world_bounds.stretch(coords, map.info.width, map.info.height),
            _ => (),
        });
        let width = (world_bounds.max.x - world_bounds.min.x) as usize;
        let height = (world_bounds.max.y - world_bounds.min.y) as usize;
        let overworld_info = OverworldInfo {
            width,
            height,
            pic_path: "maps/overworld.png".into(),
        };

        // Offset overworld maps so that they are always positive
        maps.iter_mut()
            .flatten()
            .for_each(|map| match map.info.coords {
                Some(coords) => {
                    let offset_coords = SpriteCoords {
                        x: coords.x - world_bounds.min.x,
                        y: coords.y - world_bounds.min.y,
                    };
                    assert!(offset_coords.x >= 0 && offset_coords.y >= 0);
                    map.info.coords = Some(offset_coords);
                }
                _ => (),
            });

        Self {
            maps,
            overworld_info,
        }
    }

    // Estimate number of trainers by looking at the
    // trainer class ids used in every map
    pub fn get_trainers_count(&self) -> usize {
        let max_id = self
            .maps
            .iter()
            .flatten()
            .flat_map(|map| map.trainer_class_ids.iter().collect::<HashSet<_>>())
            .fold(0, |max, id| cmp::max(max, *id as usize));

        assert!(max_id > 0 && max_id < 255);
        max_id + 1
    }

    pub fn export_pictures(&self, outdir: &Path) -> Result<(), Box<dyn Error>> {
        let maps_dir = outdir.join("maps");
        fs::create_dir_all(&maps_dir)?;

        // Save individual maps
        for map in self.maps.iter().flatten() {
            map.export_picture(outdir)?;
        }

        // Build the overworld picture
        let width = self.overworld_info.width * SPRITE_PIXELS_1D;
        let height = self.overworld_info.height * SPRITE_PIXELS_1D;
        let mut overworld = Image24bpp::new(width, height);
        self.maps
            .iter()
            .flatten()
            .for_each(|map| match map.info.coords {
                Some(coords) => overworld.blend(
                    &map.pic,
                    (coords.x as usize) * SPRITE_PIXELS_1D,
                    (coords.y as usize) * SPRITE_PIXELS_1D,
                ),
                _ => (),
            });

        // Save overworld
        overworld.save_to_png(&outdir.join(&self.overworld_info.pic_path))?;

        Ok(())
    }

    pub fn export_maps_info(&self) -> Vec<Option<&MapInfo>> {
        self.maps
            .iter()
            .map(|map| match map {
                Some(m) => Some(&m.info),
                _ => None,
            })
            .collect()
    }
}
