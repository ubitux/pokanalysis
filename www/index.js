"use strict";

const dir = "out";

async function fetchData() {
  const request_url = dir + "/data.json";
  const request = new Request(request_url);
  const response = await fetch(request);
  return await response.text();
}

// XXX can we avoid the parsing? It seems we can't store the objects
// inside the cache
// XXX ctrl-shift-R doesn't actually trash the session storage, we
// really need to close and re-open the tab
// XXX is there a risk of race conditions in this call?
async function getData() {
  const cached_data = sessionStorage.getItem("data");
  if (!cached_data) {
    const data = await fetchData();
    sessionStorage.setItem("data", data);
    return JSON.parse(data);
  }
  return JSON.parse(cached_data);
}

async function populate() {
  // Preferred default view
  show_maps();

  // Pokédex
  const data = await getData();
  for (let dex_id = 1; dex_id <= data.pokedex.length; dex_id++)
    pokedex_grid.appendChild(getPkmnBox(data.pokedex, dex_id));
  loadPokemonInfo(0);

  // Maps
  loadMap(0, 0xff, 0);
  maps_img.addEventListener("mousedown", mapPanningStart);
  maps_img.addEventListener("mouseup", mapPanningStop);
  maps_img.addEventListener("mouseleave", mapPanningStop);
  maps_img.addEventListener("mousemove", mapPanningMove);
}

let cursorStartPos = undefined;
let refPos = [0, 0];
let lastOutsideMap = 0;

function getCursorPos(e) {
  return [e.clientX, e.clientY];
}

function mapPanningStart(e) {
  // When we start panning, we need a reference point to compute the move vector.
  cursorStartPos = getCursorPos(e);
}

function updateMarginsFromEvent(e) {
  const cursorCurrentPos = getCursorPos(e);
  // The map picture is re-centered by the CSS so we need to ×2 the margins
  const x = refPos[0] + (cursorCurrentPos[0] - cursorStartPos[0]) * 2;
  const y = refPos[1] + (cursorCurrentPos[1] - cursorStartPos[1]) * 2;
  return updateMargins(x, y);
}

function updateMargins(x, y) {
  maps_img.style.marginLeft = `${x}px`;
  maps_img.style.marginTop = `${y}px`;
  return [x, y];
}

// XXX should we trigger than when the move leaves the zone? it's kinda broken
// when going too fast outbound
function mapPanningStop(e) {
  if (cursorStartPos === undefined)
    return;
  refPos = updateMarginsFromEvent(e);
  cursorStartPos = undefined;
}

function mapPanningMove(e) {
  if (cursorStartPos === undefined)
    return;
  updateMarginsFromEvent(e);
}

function getAreaFromElement(map, elem, tooltip_id) {
  const coords = map.coords === null ? {"x": 0, "y": 0} : map.coords;
  const x = (coords.x + elem.pos[0]) * 16;
  const y = (coords.y + elem.pos[1]) * 16;
  const area = document.createElement("area");
  area.setAttribute("coords", `${x},${y},${x+16},${y+16}`);

  area.addEventListener("mouseenter", function(e) {
    setTooltipVisibility(e, tooltip_id, true);
  });
  area.addEventListener("mouseleave", function(e) {
    setTooltipVisibility(e, tooltip_id, false);
  });
  return area;
}

function getAreaFromWarp(map, mapId, warp, tooltip_id) {
  const area = getAreaFromElement(map, warp, tooltip_id);
  area.setAttribute("onclick", `loadMap(${mapId}, ${warp.to_map}, ${warp.to_warp})`);
  area.href = "#";
  return area;
}

function getTooltipFromWarp(warp, tooltip_id) {
  const tooltip = document.createElement("div");
  tooltip.setAttribute("id", `maps_tooltip_${tooltip_id}`);
  tooltip.setAttribute("class", "tooltip tooltip_warp");
  tooltip.innerText = `To map 0x${warp.to_map.toString(16)} at warp ${warp.to_warp}`;
  return tooltip;
}

function getTooltipFromSign(sign, tooltip_id) {
  const tooltip = document.createElement("div");
  tooltip.setAttribute("id", `maps_tooltip_${tooltip_id}`);
  tooltip.setAttribute("class", "tooltip tooltip_sign");
  tooltip.innerText = sign.text;
  return tooltip;
}

function getTooltipFromEntity(entity, data, tooltip_id) {
  const tooltip = document.createElement("div");
  tooltip.setAttribute("id", `maps_tooltip_${tooltip_id}`);

  const normal_ppl = entity.data.NormalPeople;
  const trainer = entity.data.Trainer;
  const pokemon = entity.data.Pokemon;
  const item = entity.data.Item;

  if (normal_ppl) {
    tooltip.setAttribute("class", "tooltip tooltip_npc");
    tooltip.innerText = normal_ppl.text;
  } else if (trainer) {
    tooltip.setAttribute("class", "tooltip tooltip_trainer");
    const cls = data.trainers[trainer.class_id];
    const h1 = tooltip.appendChild(document.createElement("h1"));
    const img = tooltip.appendChild(document.createElement("img"));
    const spriteURL = dir + '/' + cls.sprite_path;
    h1.innerHTML = cls.name;
    img.src = spriteURL;

    const lastMonLevel = trainer.team[trainer.team.length - 1].level;
    const money = cls.base_money * lastMonLevel;
    const p = tooltip.appendChild(document.createElement("p"));
    p.innerHTML = `<b>Money:</b> $${money}`;

    const teamContainer = tooltip.appendChild(document.createElement("div"));
    teamContainer.setAttribute("class", "trainer_team");
    for (const pkmn of trainer.team) {
      const div = teamContainer.appendChild(getPkmnBox(data.pokedex, pkmn.dex_id));
      const p = div.appendChild(document.createElement("p"));
      p.innerText = `Level ${pkmn.level}`;
    }
  } else if (pokemon) {
      tooltip.setAttribute("class", "tooltip tooltip_pkmn");
      const div = tooltip.appendChild(getPkmnBox(data.pokedex, pokemon.dex_id));
      const p = div.appendChild(document.createElement("p"));
      p.innerText = `Level ${pokemon.level}`;
  } else if (item) {
    tooltip.setAttribute("class", "tooltip tooltip_item");
    if (item.text) {
      tooltip.innerText = item.text;
    } else {
      tooltip.innerHTML = `<b>Item</b>: ${item.name}`;
    }
  }

  return tooltip;
}

function getTooltipFromHidden(hidden, tooltip_id) {
  const tooltip = document.createElement("div");
  tooltip.setAttribute("id", `maps_tooltip_${tooltip_id}`);
  tooltip.setAttribute("class", "tooltip tooltip_hidden");
  if (hidden.content) {
    tooltip.innerHTML = `<b>Hidden item:</b> ${hidden.content}`;
  } else {
    tooltip.innerText = "<Special>";
  }
  return tooltip;
}

function setTooltipVisibility(e, tooltip_id, visible) {
  const display = visible ? "block" : "none";
  const tooltip = document.getElementById(`maps_tooltip_${tooltip_id}`);
  tooltip.style.display = display;
  tooltip.style.left = `${e.clientX+10}px`;
  tooltip.style.top = `${e.clientY+10}px`;
}

function registerItems(map, data, tooltip_id) {
  const mapId = data.maps.indexOf(map); // XXX kinda ugly; pass it down?
  for (const warp of map.warps) {
    maps_map.appendChild(getAreaFromWarp(map, mapId, warp, tooltip_id));
    maps_tooltips.appendChild(getTooltipFromWarp(warp, tooltip_id));
    tooltip_id++;
  }
  for (const sign of map.signs) {
    maps_map.appendChild(getAreaFromElement(map, sign, tooltip_id));
    maps_tooltips.appendChild(getTooltipFromSign(sign, tooltip_id));
    tooltip_id++;
  }
  for (const entity of map.entities) {
    maps_map.appendChild(getAreaFromElement(map, entity, tooltip_id));
    maps_tooltips.appendChild(getTooltipFromEntity(entity, data, tooltip_id));
    tooltip_id++;
  }
  for (const hidden of map.hiddens) {
    maps_map.appendChild(getAreaFromElement(map, hidden, tooltip_id));
    maps_tooltips.appendChild(getTooltipFromHidden(hidden, tooltip_id));
    tooltip_id++;
  }
  return tooltip_id;
}

// XXX is async needed?
async function loadMap(curId, mapId, warpId) {
  const data = await getData();

  maps_map.innerHTML = "";
  maps_tooltips.innerHTML = "";

  if (data.maps[curId].coords !== null)
    lastOutsideMap = curId;
  else if (mapId != 0xff && data.maps[mapId].coords !== null)
    lastOutsideMap = mapId;

  if (mapId == 0xff) {
    // FIXME this is problematic if the submap is wrong
    // (typically when the last map address is overriden by a script)
    const subMap = data.maps[lastOutsideMap];
    const warp = subMap.warps[warpId];
    const x = (subMap.coords.x + warp.pos[0] - data.overworld.width / 2) * 16;
    const y = (subMap.coords.y + warp.pos[1] - data.overworld.width / 2) * 16;
    refPos = updateMargins(-x * 2, -y * 2);

    const overworld = data.overworld;
    maps_img.src = dir + '/' + overworld.pic_path;

    let tooltip_id = 0;
    for (const map of data.maps) {
      if (map === null || map.coords === null)
        continue;
      tooltip_id = registerItems(map, data, tooltip_id);
    }
  } else {
    refPos = updateMargins(0, 0);

    const map = data.maps[mapId];
    maps_img.src = dir + '/' + map.pic_path;

    registerItems(map, data, 0);
  }
}

function getPkmnBox(pokedex, dex_id) {
  const pkmn = pokedex[dex_id - 1];

  const div = document.createElement("div");
  div.setAttribute("class", "pkmn");
  div.setAttribute("onclick", `loadPokemonInfo(${dex_id - 1})`)

  const img = div.appendChild(document.createElement("img"));
  img.src = dir + '/' + pkmn.sprite_front_path;

  const name = div.appendChild(document.createElement("p"));
  name.setAttribute("class", "name");
  name.innerText = `#${dex_id} ${pkmn.name}`;

  return div;
}

async function loadPokemonInfo(i) {
  const data = await getData();
  const pkmn = data.pokedex[i];

  front_sprite.src = dir + '/' + pkmn.sprite_front_path;
  back_sprite.src = dir + '/' + pkmn.sprite_back_path;
  field_name.innerText = `#${i + 1} ${pkmn.name}`;
  for (const field of ["species_name", "weight", "height", "growth_rate", "desc"]) {
    document.getElementById("field_" + field).innerText = pkmn[field];
  }
  field_types.innerText = pkmn.types.join(", ");
  for (const field of ["hp", "atk", "def", "spd", "spe", "cap", "exp"]) {
    document.getElementById(field).value = pkmn[field];
  }

  const evos_hdr_display = pkmn.evolutions.length ? "initial" : "none";
  evos_container.style.display = evos_hdr_display;
  evos.innerHTML = "";
  for (const evo of pkmn.evolutions) {
    const levelEvo = evo.Level;
    const xchgEvo = evo.Exchange;
    const stoneEvo = evo.Stone;
    if (levelEvo) {
      const div = evos.appendChild(getPkmnBox(data.pokedex, levelEvo.pkmn_id));
      const p = div.appendChild(document.createElement("p"));
      p.innerText = `Level ${levelEvo.level}`;
    } else if (xchgEvo) {
      const div = evos.appendChild(getPkmnBox(data.pokedex, xchgEvo.pkmn_id));
      const p = div.appendChild(document.createElement("p"));
      p.innerText = "Exchange";
    } else if (stoneEvo) {
      const div = evos.appendChild(getPkmnBox(data.pokedex, stoneEvo.pkmn_id));
      const p = div.appendChild(document.createElement("p"));
      p.innerText = stoneEvo.stone;
    }
  }

  attacks.innerHTML = "";
  for (const atk of pkmn.attacks) {
    const tr = attacks.appendChild(document.createElement("tr"));
    const tdLevel = tr.appendChild(document.createElement("td"));
    const tdMove = tr.appendChild(document.createElement("td"));
    tdLevel.innerHTML = atk[0] ? atk[0] : "<i>Native</i>";
    tdMove.innerHTML = `<b>${atk[1]}</b>`;
  }

  tmhm.innerHTML = "";
  for (const m of pkmn.tmhm) {
    const tr = tmhm.appendChild(document.createElement("tr"));
    const tdId = tr.appendChild(document.createElement("td"));
    const tdName = tr.appendChild(document.createElement("td"));
    tdId.innerText = m[1];
    tdName.innerHTML = `<b>${m[0]}</b>`;
  }
}

function show_maps() {
  maps_tab.style.display = "initial";
  pokedex_tab.style.display = "none";
  btn_maps.disabled = true;
  btn_pdex.disabled = false;
}

function show_pokedex() {
  maps_tab.style.display = "none";
  pokedex_tab.style.display = "flex";
  btn_maps.disabled = false;
  btn_pdex.disabled = true;
}