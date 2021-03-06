#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright © 2010-2011, Clément Bœsch
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

import sys, gtk, pango, re

try:
    import pokerom
except ImportError:
    print('Unable to load module pokerom. Try to compile it with `make`')
    sys.exit(1)

class Core:

    def get_map_pix(self, pkmn_map):
        p1, p2, p3 = [None] * 3
        if pkmn_map['map_pic']:
            w = pkmn_map['map_w'] * 32
            h = pkmn_map['map_h'] * 32
            p1 = gtk.gdk.pixbuf_new_from_data(pkmn_map['map_pic'], gtk.gdk.COLORSPACE_RGB, False, 8, w, h, w * 3)
            p2 = p1.scale_simple(150, 150, gtk.gdk.INTERP_BILINEAR)
            p3 = p2.scale_simple(20, 20, gtk.gdk.INTERP_BILINEAR)
        return p1, p2, p3

    def get_map_str(self, i):
        return 'Map #%03d' % i

    def load_world(self):
        model = self.gui.get_object('treestore_gameinfo')
        self.map_store = {}
        area_id = 1
        for pkmn_map in self.rom.get_maps():
            (p1, p2, p3) = self.get_map_pix(pkmn_map)
            if len(pkmn_map['info']) > 1:
                self.map_store['Area #%d' % area_id] = (pkmn_map, p1, p2)
                area = model.append(None, (p3, 'Area #%d' % area_id, '%d maps' % len(pkmn_map['info'])))
                self.map_nodes['Area #%d' % area_id] = area
                area_id += 1
                for info in pkmn_map['info']:
                    (p1, p2, p3) = self.get_map_pix(info)
                    self.map_store[self.add_info(info, p3, area)] = (info, p1, p2)
            else:
                data = pkmn_map['info'][0]
                self.map_store[self.add_info(data, p3)] = (data, p1, p2)
        self.select_map('Area #1')

    def add_info(self, info, pic, area=None):
        model = self.gui.get_object('treestore_gameinfo')
        map_id = self.get_map_str(info['id'])

        ### Map Header
        map_boxes_y, map_boxes_x = info['map_h'] * 2, info['map_w'] * 2

        self.map_nodes[map_id] = model_map_node = model.append(area, (pic, map_id, '%02X:%04X' % (info['bank-id'], info['addr'])))
        model.append(model_map_node, (None, 'tileset', '0x%02X' % info['tileset']))
        model.append(model_map_node, (None, 'dimensions', '%dx%d ' % (info['map_w'], info['map_h'])))
        model.append(model_map_node, (None, 'map pointer', '%02X:%04X' % (info['bank-id'], info['map-pointer'])))
        model.append(model_map_node, (None, 'text pointer', '%02X:%04X' % (info['bank-id'], info['map-text-pointer'])))
        model.append(model_map_node, (None, 'script pointer', '%02X:%04X' % (info['bank-id'], info['map-script-pointer'])))

        # Connections
        connections = info['connections']
        model_connections_node = model.append(model_map_node, (None, '%d Connections' % len(connections), '-'.join(c['key'] for c in connections)))
        for c_id, c in enumerate(connections):
            c_info = info['connections'][c_id]
            model_connection_node = model.append(model_connections_node, (None, c_info['key'], ''))
            model.append(model_connection_node, (None, 'index', c_info['index']))
            model.append(model_connection_node, (None, 'connected map', '%02X:%04X' % (info['bank-id'], c_info['connected-map'])))
            model.append(model_connection_node, (None, 'current map', '%02X:%04X' % (info['bank-id'], c_info['current-map'])))
            model.append(model_connection_node, (None, 'bigness', c_info['bigness']))
            model.append(model_connection_node, (None, 'map width', c_info['map_width']))
            model.append(model_connection_node, (None, 'x align', c_info['x_align']))
            model.append(model_connection_node, (None, 'y align', c_info['y_align']))
            model.append(model_connection_node, (None, 'window', '%02X:%04X' % (info['bank-id'], c_info['window'])))

        ### Object Data
        model_object_data_node = model.append(model_map_node, (None, 'Object data', '%02X:%04X' % (info['bank-id'], info['object-data'])))
        model.append(model_object_data_node, (None, 'maps border tile', '0x%02x' % info['maps_border_tile']))

        # Warps
        warps_info = info['warps']
        model_warps_node = model.append(model_object_data_node, (None, 'Warps', '(%d)' % len(warps_info)))
        for (warp, warp_data) in enumerate(warps_info):
            model_warp_node = model.append(model_warps_node, (None, warp, ''))
            model.append(model_warp_node, (None, 'x', '%02d/%02d' % (warp_data['x'], map_boxes_x)))
            model.append(model_warp_node, (None, 'y', '%02d/%02d' % (warp_data['y'], map_boxes_y)))
            model.append(model_warp_node, (None, 'to point', '0x%02x' % warp_data['to_point']))
            model.append(model_warp_node, (None, 'to map', '0x%02x' % warp_data['to_map']))

        # Signs
        signs_info = info['signs']
        model_signs_node = model.append(model_object_data_node, (None, 'Signposts', '(%d)' % len(signs_info)))
        for sign, sign_data in enumerate(signs_info):
            model_sign_node = model.append(model_signs_node, (None, sign, ''))
            model.append(model_sign_node, (None, 'x', '%02d/%02d' % (sign_data['x'], map_boxes_x)))
            model.append(model_sign_node, (None, 'y', '%02d/%02d' % (sign_data['y'], map_boxes_y)))
            model.append(model_sign_node, (None, 'text id', '%d' % sign_data['text_id']))

        # Entities
        entities_info = info['entities']
        model_entities_node = model.append(model_object_data_node, (None, 'Entities', '(%d)' % len(entities_info)))
        for entity, entity_data in enumerate(entities_info):
            model_entity_node = model.append(model_entities_node, (None, entity, ''))
            model.append(model_entity_node, (None, 'picture id', '%d' % entity_data['pic_id']))
            model.append(model_entity_node, (None, 'x', '%02d/%02d' % (entity_data['x'], map_boxes_x)))
            model.append(model_entity_node, (None, 'y', '%02d/%02d' % (entity_data['y'], map_boxes_y)))
            model.append(model_entity_node, (None, 'movement 1', '%d' % entity_data['mvt_1']))
            model.append(model_entity_node, (None, 'movement 2', '%d' % entity_data['mvt_2']))
            model.append(model_entity_node, (None, 'text id', '%d' % entity_data['text_id']))
            if 'item_id' in entity_data:
                model.append(model_entity_node, (None, 'item id', '%d' % entity_data['item_id']))
            elif 'trainer_type' in entity_data:
                model.append(model_entity_node, (None, 'trainer type', '%d' % entity_data['trainer_type']))
                model.append(model_entity_node, (None, 'pkmn set', '%d' % entity_data['pkmn_set']))

        return map_id

    def select_map(self, map_id, set_focus=False):
        if map_id not in self.map_store:
            if map_id == self.get_map_str(255):
                map_id = 'Area #1'
            else:
                return

        if set_focus:
            tview = self.gui.get_object('treeview_gameinfo')
            path = tview.get_model().get_path(self.map_nodes[map_id])
            tview.set_cursor(path)
            tview.scroll_to_cell(path, use_align=True, row_align=.5)

        (data, p1, p2) = self.map_store[map_id]
        self.current_map = data['objects']
        wild_pkmn = data.get('wild-pkmn')
        self.gui.get_object('image_map').set_from_pixbuf(p1)
        self.gui.get_object('image_mini_map').set_from_pixbuf(p2)

        for place in ('grass', 'water'):
            model = self.gui.get_object('liststore_%s_wild_pkmn' % place)
            model.clear()
            if wild_pkmn and wild_pkmn.get(place):
                for (level, i) in wild_pkmn[place]:
                    pkmn = self.pokedex_rom_id[i]
                    model.append((pkmn['fmt_name'], level, pkmn['id'], pkmn['pic'][0]))

        model = self.gui.get_object('liststore_specials')
        model.clear()
        for (y, x, item_id, itype, faddr) in data.get('special-items', []):
            model.append(('<b>%s</b>' % str(item_id).title(), x, y, '0x%02X' % itype, '0x%04X' % faddr))

    def select_pkmn(self, pkmn_id):
        tview = self.gui.get_object('treeview_pokedex')
        tview.set_cursor(pkmn_id - 1)

    def on_combobox_asm_bank_changed(self, combobox):
        bank_id = combobox.get_active()
        tbuffer = self.gui.get_object('textview_asm').get_buffer()
        if bank_id not in self.bank_asm:
            self.bank_asm[bank_id] = self.rom.disasm(bank_id)
        tbuffer.set_text(self.bank_asm[bank_id])

    def on_eventbox_image_map_button_press_event(self, widget, event):
        coords = (int(event.x / 16), int(event.y / 16))
        if coords in self.current_map:
            info_data = self.current_map[coords]
            map_id = info_data.get('to_map', None)
            if map_id is not None:
                self.select_map(self.get_map_str(map_id), True)

    def on_eventbox_image_map_leave_notify_event(self, *p):
        self.gui.get_object('frame_map').hide()

    def on_eventbox_image_map_motion_notify_event(self, widget, event):
        self.gui.get_object('frame_map').show()
        (x, y) = coords = (int(event.x / 16), int(event.y / 16))
        info = [('X', x), ('Y', y)]
        if coords in self.current_map:
            info += sorted(self.current_map[coords].items())
        if 'to_map' in dict(info).keys():
            widget.window.set_cursor(self.focus_cursor)
        else:
            widget.window.set_cursor(None)
        markup_info = []
        for k, v in info:
            if k in ('x', 'y'):
                k = 'map %c' % k
            markup_info.append('<b>%s</b>: %s' % (k.replace('_', ' ').title(), v))
        self.gui.get_object('label_mapinfo').set_markup('\n'.join(markup_info))

    def on_iconview_evolutions_item_activated(self, iconview, path):
        model = iconview.get_model()
        self.select_pkmn(self.pokedex_rom_id[model.get_value(model.get_iter(path), 0)]['id'])

    def on_iconview_trainer_team_activated(self, iconview, path):
        model = iconview.get_model()
        self.on_iconview_evolutions_item_activated(iconview, path)
        self.gui.get_object('notebook_main').set_current_page(0)

    def on_treeview_gameinfo_cursor_changed(self, treeview):
        (path, column_focus) = treeview.get_cursor()
        model = treeview.get_model()
        self.select_map(model.get_value(model.get_iter(path), 1))

    def on_treeview_gameinfo_row_activated(self, treeview, path, column):
        model = treeview.get_model()
        value = model.get_value(model.get_iter(path), 2)
        if re.match("[A-F0-9]{2}:[A-F0-9]{4}", value):
            (bank_id, addr) = value.split(':')
            bank_id = int(bank_id, 16)
            self.gui.get_object('notebook_main').set_current_page(3)
            self.gui.get_object('combobox_asm_banks').set_active(bank_id)
            text_buffer = self.gui.get_object('textview_asm').get_buffer()
            text_start_iter = text_buffer.get_start_iter()
            goto_str = 'RO%s%X:%s' % ('M' if bank_id < 0x10 else '', bank_id, addr)
            match = text_start_iter.forward_search(goto_str, 0)
            if match:
                text_buffer.select_range(*match)
                mark = text_buffer.create_mark('end', match[0], False)
                self.gui.get_object('textview_asm').scroll_to_mark(mark, 0.05, True, 0.0, 0.1)

    def on_treeview_pokedex_cursor_changed(self, treeview):
        (path, column_focus) = treeview.get_cursor()
        pkmn = self.pokedex[path[0]]
        pic = pkmn['pic'][0].scale_simple(160, 160, gtk.gdk.INTERP_NEAREST)
        pic_back = pkmn['pic'][1].scale_simple(160, 160, gtk.gdk.INTERP_NEAREST)
        self.gui.get_object('image_pkmn').set_from_pixbuf(pic)
        self.gui.get_object('image_pkmn_back').set_from_pixbuf(pic_back)

        types_str = 'Type%s: <b>%s</b>' % ('s' if pkmn['types'][1] else '', '/'.join(t.title() for t in pkmn['types'] if t))
        self.gui.get_object('label_pkmn_types').set_markup(types_str)
        self.gui.get_object('label_pkmn_growth_rate').set_markup('Growth rate: <b>%s</b>' % pkmn['growth_rate'])

        self.gui.get_object('textview_pkmn_info_small').get_buffer().set_text('%s\nHeight: %s\nWeight: %s\n\n%s' %
                (pkmn['class'], pkmn['height'], pkmn['weight'], pkmn['desc']))
        self.gui.get_object('textview_pkmn_info').get_buffer().set_text(
                'ROM ID: 0x%02X\nROM header addr: 0x%06x\n' % (pkmn['rom_id'], pkmn['rom_header_addr']))
        self.gui.get_object('label_pkmn').set_markup('#%s <b>%s</b>' % (pkmn['fmt_id'], pkmn['name']))

        for stat in ('HP', 'ATK', 'DEF', 'SPD', 'SPE', 'EXP', 'CAP'):
            value = pkmn['stats'][stat]
            pgbar = self.gui.get_object('progressbar_stat_' + stat)
            pgbar.set_fraction(value / 255.)
            pgbar.set_text('%d' % value)

        atk_lstore = self.gui.get_object('liststore_pkmn_attacks')
        atk_lstore.clear()
        for (level, atk) in pkmn['attacks']:
            atk_lstore.append(('<i>Native</i>' if not level else '%d' % level, '<b>%s</b>' % atk.title()))

        HMTM_lstore = self.gui.get_object('liststore_pkmn_HM_TM')
        HMTM_lstore.clear()
        for HMTM, move in pkmn['HM_TM']:
            HMTM_lstore.append(('<b>%s</b>' % HMTM, '<i>%s</i>' % move.title()))

        evol_lstore = self.gui.get_object('liststore_evolutions')
        evol_lstore.clear()
        for evol in pkmn['evolutions']:
            pkmn_id = evol['pkmn-id']
            pkmn = self.pokedex_rom_id[pkmn_id]
            txt = evol['type']
            if txt == 'level':
                txt += ' %d' % evol['level']
            elif txt == 'stone':
                txt = evol['stone']
            evol_lstore.append((pkmn_id, pkmn['pic'][0], pkmn['fmt_name'], txt.title()))

    def on_treeview_trainers_cursor_changed(self, treeview):
        (path, column_focus) = treeview.get_cursor()
        trainer = self.trainers[path[0]]
        trainerteam_lstore = self.gui.get_object('liststore_trainer_team')
        trainerteam_lstore.clear()
        for pkmn_id, lvl in trainer.get('team', []):
            pkmn  = self.pokedex_rom_id[pkmn_id]
            trainerteam_lstore.append((pkmn_id, pkmn['pic'][0], pkmn['fmt_name'], 'lvl %d' % lvl))

    def on_treeview_wild_pkmn_row_activated(self, treeview, path, column):
        self.gui.get_object('notebook_main').set_current_page(0)
        model = treeview.get_model()
        self.gui.get_object('treeview_pokedex').set_cursor(model.get_value(model.get_iter(path), 2) - 1)

    def __init__(self):
        if len(sys.argv) != 2:
            print('Usage: %s file' % sys.argv[0])
            return

        self.rom = pokerom.ROM(sys.argv[1])

        self.focus_cursor = gtk.gdk.Cursor(gtk.gdk.HAND2)

        self.gui = gtk.Builder()
        self.gui.add_from_file('gui.glade')
        self.gui.connect_signals({
            'on_combobox_asm_bank_changed': self.on_combobox_asm_bank_changed,
            'on_eventbox_image_map_button_press_event': self.on_eventbox_image_map_button_press_event,
            'on_eventbox_image_map_leave_notify_event': self.on_eventbox_image_map_leave_notify_event,
            'on_eventbox_image_map_motion_notify_event': self.on_eventbox_image_map_motion_notify_event,
            'on_iconview_evolutions_item_activated': self.on_iconview_evolutions_item_activated,
            'on_iconview_trainer_team_activated': self.on_iconview_trainer_team_activated,
            'on_treeview_gameinfo_cursor_changed': self.on_treeview_gameinfo_cursor_changed,
            'on_treeview_gameinfo_row_activated': self.on_treeview_gameinfo_row_activated,
            'on_treeview_pokedex_cursor_changed': self.on_treeview_pokedex_cursor_changed,
            'on_treeview_trainers_cursor_changed': self.on_treeview_trainers_cursor_changed,
            'on_treeview_wild_pkmn_row_activated': self.on_treeview_wild_pkmn_row_activated,
            'on_window_destroy': lambda w: gtk.main_quit(),
        })

        self.pokedex_rom_id = {}
        self.pokedex = self.rom.get_pokedex()
        pokedex_widget = self.gui.get_object('liststore_pokedex')
        w, h = 7 * 8, 7 * 8
        for i, pkmn in enumerate(self.pokedex, 1):
            pic = pkmn['pic']
            pkmn['pic'] = (
                    gtk.gdk.pixbuf_new_from_data(pic[0], gtk.gdk.COLORSPACE_RGB, False, 8, w, h, w * 3),
                    gtk.gdk.pixbuf_new_from_data(pic[1], gtk.gdk.COLORSPACE_RGB, False, 8, w, h, w * 3)
            )
            pkmn['fmt_name'] = '<b>%s</b>' % pkmn['name'].title()
            pkmn['fmt_id'] = '%03d' % i
            pkmn['fmt_rom_id'] = '0x%02X' % pkmn['rom_id']
            pokedex_widget.append((pkmn['fmt_id'], pkmn['fmt_name'], pkmn['pic'][0]))
            self.pokedex_rom_id[pkmn['rom_id']] = pkmn
        self.gui.get_object('treeview_pokedex').set_cursor((0,))

        self.trainers = self.rom.get_trainers()
        trainers_widget = self.gui.get_object('liststore_trainers')
        for i, trainer in enumerate(self.trainers):
            if 'pkmn_id' in trainer:
                pkmn_id = trainer['pkmn_id']
                name    = self.pokedex_rom_id[pkmn_id]['name']
                pic     = self.pokedex_rom_id[pkmn_id]['pic'][0]
                extra   = 'level %d' % trainer['level']
            else:
                name  = trainer['name']
                pic   = gtk.gdk.pixbuf_new_from_data(trainer['pic'], gtk.gdk.COLORSPACE_RGB, False, 8, w, h, w * 3)
                extra = '%d pkmn' % len(trainer['team'])
            trainers_widget.append(('<b>%s</b>' % name.title(), pic, extra))

        monospaced_font = pango.FontDescription('monospace')
        self.gui.get_object('textview_asm').modify_font(monospaced_font)
        self.gui.get_object('textview_pkmn_info').modify_font(monospaced_font)
        self.gui.get_object('textview_pkmn_info_small').modify_font(monospaced_font)

        self.bank_asm = {}
        lss_banks = self.gui.get_object('liststore_asm_banks')
        for bank_id in range(64):
            lss_banks.append(('Bank %02X' % bank_id,))
        self.gui.get_object('combobox_asm_banks').set_active(0)

        self.map_nodes = {}
        self.load_world()

        self.run()

    def run(self):
        win = self.gui.get_object('window')
        win.show()
        gtk.main()

Core()
