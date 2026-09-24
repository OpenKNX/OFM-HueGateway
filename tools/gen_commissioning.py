# -*- coding: utf-8 -*-
"""Erzeugt src/HueGatewayModule.commissioning.parts.xml - die Seite "Inbetriebnahme".

Die Seite startet das Pairing mit der Bridge (Button, online), liest die Hue-Geräte über die Bridge aus (Button, online), zeigt sie gruppiert
pro Gerät mit allen IDs und überträgt die gewählten Zuordnungen in Kanäle und
Szenen-Slots (Button, offline). Logik: src/HueGatewayModule.script.js,
Gegenstelle im Gerät: HueGatewayModule::processFunctionProperty.

Alle Parameter dieser Seite sind speicherlos (kein <Memory>): sie leben nur im
ETS-Projekt und werden nie ins Gerät geladen.

Aufruf: python tools/gen_commissioning.py
"""
import io
import os

# Muss zu kEtsMax* in HueGatewayModule.cpp passen.
MAX_LIGHTS = 64
MAX_ACCESSORIES = 32
MAX_SCENES = 64
NUM_CHANNELS = 32
NUM_SCENE_SLOTS = 8
ROWS_PER_TABLE = 16

P = "%"
AID = P + "AID" + P
TT = P + "TT" + P
PS = AID + "_PS-nnn"

# Parameter-Nummern: 1rrcc Leuchten, 2rrcc Zubehör, 3rrcc Szenen, 001xx Sonstiges.
STATUS = 100
RESULT = 101
LIGHT_COUNT = 102
ACCESSORY_COUNT = 103
SCENE_COUNT = 104
PAIR_STATUS = 105

# (Spaltenname, Überschrift, Breite, Parametertyp, editierbar)
LIGHT_COLUMNS = [
    ("Name", "Name", "14%", "HUECommName", False),
    ("Id", "Leuchten-ID", "16%", "HUECommId", False),
    ("Room", "Raum", "10%", "HUECommName", False),
    ("RoomId", "Raum-ID", "16%", "HUECommId", False),
    ("Zone", "Zone", "10%", "HUECommName", False),
    ("ZoneId", "Zone-ID", "16%", "HUECommId", False),
    ("Channel", "Kanal", "9%", "HUECommChannel", True),
    ("Target", "Steuern als", "9%", "HUECommTarget", True),
]
ACCESSORY_COLUMNS = [
    ("Name", "Name", "25%", "HUECommName", False),
    ("Type", "Typ", "20%", "HUECommAccessoryType", False),
    ("Id", "ID", "40%", "HUECommId", False),
    ("Channel", "Kanal", "15%", "HUECommChannel", True),
]
SCENE_COLUMNS = [
    ("Name", "Name", "25%", "HUECommName", False),
    ("Room", "Raum", "20%", "HUECommName", False),
    ("Id", "Szenen-ID", "40%", "HUECommId", False),
    ("Slot", "Szenen-Slot", "15%", "HUECommSceneSlot", True),
]

# (Kürzel, Nummernbasis, Zeilen, Spalten, Überschrift, Zählparameter)
TABLES = [
    ("L", 10000, MAX_LIGHTS, LIGHT_COLUMNS, "Leuchten", LIGHT_COUNT),
    ("A", 20000, MAX_ACCESSORIES, ACCESSORY_COLUMNS, "Schalter, Sensoren und Steckdosen", ACCESSORY_COUNT),
    ("S", 30000, MAX_SCENES, SCENE_COLUMNS, "Szenen", SCENE_COUNT),
]


def pid(n):
    return AID + "_P-" + TT + "%05d" % n


def pref(n):
    return pid(n) + "_R-" + TT + "%05d01" % n


def enum_type(name, entries, size=8):
    lines = ['              <ParameterType Id="%s_PT-%s" Name="%s">' % (AID, name, name),
             '                <TypeRestriction Base="Value" SizeInBit="%d" UIHint="DropDown">' % size]
    for value, text in entries:
        lines.append('                  <Enumeration Text="%s" Value="%d" Id="%s_PT-%s_EN-%d" />' % (text, value, AID, name, value))
    lines += ['                </TypeRestriction>', '              </ParameterType>']
    return lines


def text_type(name, size_bytes):
    return ['              <ParameterType Id="%s_PT-%s" Name="%s">' % (AID, name, name),
            '                <TypeText SizeInBit="%d" />' % (size_bytes * 8),
            '              </ParameterType>']


def parameter_types():
    lines = []
    lines += text_type("HUECommName", 64)
    lines += text_type("HUECommId", 40)
    lines += text_type("HUECommStatus", 120)
    lines += text_type("HUECommResult", 8192)
    lines += ['              <ParameterType Id="%s_PT-HUECommCount" Name="HUECommCount">' % AID,
              '                <TypeNumber SizeInBit="8" Type="unsignedInt" minInclusive="0" maxInclusive="255" />',
              '              </ParameterType>']
    lines += enum_type("HUECommChannel", [(0, "–")] + [(c, "Hue %d" % c) for c in range(1, NUM_CHANNELS + 1)])
    lines += enum_type("HUECommTarget", [(0, "Leuchte"), (1, "Raum"), (2, "Zone")])
    lines += enum_type("HUECommSceneSlot", [(0, "–")] + [(s, "Hue Szene %d" % s) for s in range(1, NUM_SCENE_SLOTS + 1)])
    lines += enum_type("HUECommAccessoryType", [
        (0, "–"),
        (1, "Taster"),
        (2, "Taster mit Drehelement"),
        (3, "Bewegungsmelder"),
        (4, "Kontaktsensor"),
        (5, "Steckdose"),
        (6, "Sonstiges"),
    ])
    return lines


def parameter(n, name, ptype, text, value, access=None):
    acc = ' Access="%s"' % access if access else ""
    return '              <Parameter Id="%s" Name="%s" ParameterType="%s_PT-%s" Text="%s" Value="%s"%s />' % (
        pid(n), name, AID, ptype, text, value, acc)


def table_params():
    """Liefert je Tabelle eine Liste von Zeilen, jede Zeile eine Liste (Nummer, Spalte)."""
    result = []
    for key, base, rows, columns, _title, _count in TABLES:
        table = []
        for r in range(1, rows + 1):
            table.append([(base + r * 100 + c, col) for c, col in enumerate(columns, 1)])
        result.append(table)
    return result


def parameters():
    lines = [
        parameter(STATUS, "HUECommStatus", "HUECommStatus", "Status", "", "None"),
        parameter(RESULT, "HUECommResult", "HUECommResult", "Übernahme-Resultat", "", "Read"),
        parameter(LIGHT_COUNT, "HUECommLightCount", "HUECommCount", "Anzahl Leuchten", "0", "None"),
        parameter(ACCESSORY_COUNT, "HUECommAccessoryCount", "HUECommCount", "Anzahl Zubehör", "0", "None"),
        parameter(SCENE_COUNT, "HUECommSceneCount", "HUECommCount", "Anzahl Szenen", "0", "None"),
        parameter(PAIR_STATUS, "HUECommPairStatus", "HUECommStatus", "Pairing-Status", "", "None"),
    ]
    for (key, _base, _rows, _columns, _title, _count), table in zip(TABLES, table_params()):
        for r, row in enumerate(table, 1):
            for n, (col, header, _width, ptype, editable) in row:
                value = "0" if ptype in ("HUECommChannel", "HUECommTarget", "HUECommSceneSlot", "HUECommAccessoryType") else ""
                lines.append(parameter(n, "HUEComm%s%02d%s" % (key, r, col), ptype, header, value, None if editable else "Read"))
    return lines


def parameter_refs():
    numbers = [STATUS, RESULT, LIGHT_COUNT, ACCESSORY_COUNT, SCENE_COUNT, PAIR_STATUS]
    for table in table_params():
        for row in table:
            numbers += [n for n, _col in row]
    return ['              <ParameterRef Id="%s" RefId="%s" />' % (pref(n), pid(n)) for n in numbers]


def table_block(key, table, columns, first, last, index):
    bid = AID + "_PB-" + TT + "C%s%d" % (key, index)
    lines = ['                    <ParameterBlock Id="%s" Name="HUEComm%sTable%d" Inline="true" Layout="Table" HelpContext="HUE-Inbetriebnahme">' % (bid, key, index),
             '                      <Rows>']
    for r in range(first, last + 1):
        lines.append('                        <Row Id="%s_R-%d" Name="R%d" Text="%d" />' % (bid, r - first + 1, r - first + 1, r))
    lines += ['                      </Rows>', '                      <Columns>']
    for c, (_col, header, width, _ptype, _editable) in enumerate(columns, 1):
        lines.append('                        <Column Id="%s_C-%d" Name="C%d" Text="%s" Width="%s" />' % (bid, c, c, header, width))
    lines.append('                      </Columns>')
    for r in range(first, last + 1):
        for c, (n, _col) in enumerate(table[r - 1], 1):
            lines.append('                      <ParameterRefRef RefId="%s" Cell="%d,%d" />' % (pref(n), r - first + 1, c))
    lines.append('                    </ParameterBlock>')
    return lines


def dynamic():
    lines = [
        '              <ParameterBlock Id="%s_PB-nnn" Name="HueCommissioning" Text="Inbetriebnahme" Icon="import" HelpContext="HUE-Inbetriebnahme">' % AID,
        '                <ParameterSeparator Id="%s" Text="Hue-Geräte laden und zuordnen" UIHint="Headline" />' % PS,
        '                <ParameterSeparator Id="%s" Text="Das Gerät muss mit dieser Applikation programmiert sein. Die Tabellen dieser Seite werden nur im ETS-Projekt gespeichert und nicht in das Gerät geladen." UIHint="Information" />' % PS,
        '                <ParameterSeparator Id="%s" Text="Es werden bis zu %d Leuchten, %d Schalter/Sensoren/Steckdosen und %d Szenen angezeigt. Größere Installationen bitte über das WebUI (Hue-Geräte laden) einrichten." UIHint="Information" />' % (PS, MAX_LIGHTS, MAX_ACCESSORIES, MAX_SCENES),
        '                <ParameterSeparator Id="%s" Text="" UIHint="HorizontalRuler" />' % PS,
        '                <ParameterSeparator Id="%s" Text="1. Hue-Bridge koppeln (Pairing)" UIHint="Headline" />' % PS,
        '                <ParameterSeparator Id="%s" Text="Startet das Pairing. Anschließend innerhalb des Pairing-Zeitfensters den Link-Button an der Hue-Bridge drücken. Ist die Bridge bereits gekoppelt, entfällt dieser Schritt." UIHint="Information" />' % PS,
        '                <Button Id="%s_B-%s00003" Text="Pairing starten" EventHandler="HUE_btnCommPair" EventHandlerOnline="ConnectionOriented" EventHandlerParameters="{ }" />' % (AID, TT),
        '                <ParameterSeparator Id="%s" Text="{{0:Pairing-Status noch nicht abgefragt.}}" TextParameterRefId="%s" />' % (PS, pref(PAIR_STATUS)),
        '                <ParameterSeparator Id="%s" Text="" UIHint="HorizontalRuler" />' % PS,
        '                <ParameterSeparator Id="%s" Text="2. Hue-Geräte von der Bridge laden" UIHint="Headline" />' % PS,
        '                <Button Id="%s_B-%s00001" Text="Hue-Geräte laden" EventHandler="HUE_btnCommLoad" EventHandlerOnline="ConnectionOriented" EventHandlerParameters="{ }" />' % (AID, TT),
        '                <ParameterSeparator Id="%s" Text="{{0:Noch keine Hue-Geräte geladen.}}" TextParameterRefId="%s" />' % (PS, pref(STATUS)),
        '                <ParameterSeparator Id="%s" Text="" UIHint="HorizontalRuler" />' % PS,
        '                <ParameterSeparator Id="%s" Text="3. Kanäle und Szenen-Slots zuordnen" UIHint="Headline" />' % PS,
        '                <ParameterSeparator Id="%s" Text="Pro Gerät den Hue-Kanal wählen, bei Leuchten zusätzlich, ob die Leuchte selbst, ihr Raum oder ihre Zone gesteuert wird. Bestehende Zuordnungen sind nach dem Laden bereits vorbelegt." UIHint="Information" />' % PS,
    ]
    for (key, _base, rows, columns, title, count), table in zip(TABLES, table_params()):
        lines.append('                <ParameterSeparator Id="%s" Text="%s" UIHint="Headline" />' % (PS, title))
        lines.append('                <choose ParamRefId="%s">' % pref(count))
        lines.append('                  <when test="0">')
        lines.append('                    <ParameterSeparator Id="%s" Text="Keine Einträge." />' % PS)
        lines.append('                  </when>')
        for index, first in enumerate(range(1, rows + 1, ROWS_PER_TABLE)):
            last = min(first + ROWS_PER_TABLE - 1, rows)
            lines.append('                  <when test="&gt;%d">' % (first - 1))
            lines += table_block(key, table, columns, first, last, index + 1)
            lines.append('                  </when>')
        lines.append('                </choose>')
    lines += [
        '                <ParameterSeparator Id="%s" Text="" UIHint="HorizontalRuler" />' % PS,
        '                <ParameterSeparator Id="%s" Text="4. Zuordnungen übernehmen" UIHint="Headline" />' % PS,
        '                <ParameterSeparator Id="%s" Text="Überträgt Gerätetyp, Zieltyp und Ziel-ID in die gewählten Kanäle und die Szenen-IDs in die Szenen-Slots. Leere Kanalbeschreibungen erhalten den Hue-Namen. Bei doppelt belegten Kanälen oder Slots wird nichts übernommen." UIHint="Information" />' % PS,
        '                <Button Id="%s_B-%s00002" Text="Zuordnungen übernehmen" EventHandler="HUE_btnCommApply" EventHandlerParameters="{ }" />' % (AID, TT),
        '                <ParameterRefRef RefId="%s" HelpContext="HUE-Inbetriebnahme" />' % pref(RESULT),
        '              </ParameterBlock>',
    ]
    return lines


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
    out = []
    out += ['<?xml version="1.0" encoding="utf-8"?>',
            '<?xml-model href="../../../../Organization/knxprod-support/knx_project_20/knx-editor.xsd" type="application/xml" schematypens="http://www.w3.org/2001/XMLSchema"?>',
            '<!-- ERZEUGT von tools/gen_commissioning.py - nicht von Hand bearbeiten. -->',
            '<KNX xmlns:op="http://github.com/OpenKNX/OpenKNXproducer" xmlns="http://knx.org/xml/project/20" CreatedBy="KNX MT" ToolVersion="5.1.255.16695">',
            '  <ManufacturerData>',
            '    <Manufacturer>',
            '      <ApplicationPrograms>',
            '        <ApplicationProgram>',
            '          <Static>',
            '            <ParameterTypes>']
    out += parameter_types()
    out += ['            </ParameterTypes>', '            <Parameters>']
    out += parameters()
    out += ['            </Parameters>', '            <ParameterRefs>']
    out += parameter_refs()
    out += ['            </ParameterRefs>',
            '          </Static>',
            '          <Dynamic>',
            '            <Channel>']
    out += dynamic()
    out += ['            </Channel>',
            '          </Dynamic>',
            '        </ApplicationProgram>',
            '      </ApplicationPrograms>',
            '    </Manufacturer>',
            '  </ManufacturerData>',
            '</KNX>',
            '']
    path = os.path.join(root, "HueGatewayModule.commissioning.parts.xml")
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print("geschrieben: " + os.path.normpath(path))


if __name__ == "__main__":
    main()
