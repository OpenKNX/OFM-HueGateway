// ---------------------------------------------------------------------------------------
// Inbetriebnahme: Bridge koppeln, Hue-Geräte aus der ETS laden und Kanälen / Szenen-Slots
// zuordnen.
// Seite und Tabellen: HueGatewayModule.commissioning.parts.xml.
//
// Gegenstelle im Gerät: HueGatewayModule::processFunctionProperty, objectIndex 0xA3,
// propertyId 1. Erstes Datenbyte ist das Kommando:
//   1 = Scan starten   -> [0,0] angenommen, [1,err] keine Bridge-Verbindung
//   2 = Status         -> [0,0] läuft, [0,1,err,Lichter,Zubehör,Szenen,Überlauf-Bits]
//   3 = Zeile abholen  -> [3,Art,Index], Art 1 Licht, 2 Zubehör, 3 Szene
//   4 = Pairing starten -> [0,0] gestartet, [1,1] nicht gestartet, [1,2] bereits gekoppelt
//   5 = Pairing-Status -> [0,Zustand,Restsekunden], Zustand 0 wartet, 1 gekoppelt, 2 fehlgeschlagen
// Das Abholen liefert bis zu 232 Byte und muss deshalb über
// BASE_invokeFunctionPropertyWrapper (OGM-Common) laufen. UUIDs kommen als 16 Byte,
// Texte als Länge + UTF-8.
//
// Die ETS-Skript-Engine ist ES3-nah: kein Array.indexOf, keine verschachtelten Literale.
// ---------------------------------------------------------------------------------------

var HUE_COMM_OBJECT = 0xA3;
var HUE_COMM_PROPERTY = 1;
var HUE_COMM_MAX_LIGHTS = 64;
var HUE_COMM_MAX_ACCESSORIES = 32;
var HUE_COMM_MAX_SCENES = 64;
var HUE_COMM_CHANNELS = 32;
var HUE_COMM_SCENE_SLOTS = 8;
var HUE_COMM_SCAN_TIMEOUT_MS = 120000;
// Pairing-Zeitfenster ist maximal 120 s, danach richtet das Gerät noch die Kanäle ein
var HUE_COMM_PAIR_TIMEOUT_MS = 180000;
var HUE_COMM_CHANNEL_NAME_BYTES = 40;
// Kanaltyp je Zubehörtyp (PT-HUECommAccessoryType -> PT-HUEChannelType), 0 = nicht zuordenbar
var HUE_COMM_ACCESSORY_CHANNEL_TYPE = [0, 3, 3, 2, 4, 5, 0];
var HUE_COMM_CHANNEL_TYPE_NAMES = ["Deaktiviert", "Licht", "Bewegungsmelder", "Taster/Schalter", "Kontaktsensor", "Steckdose"];
var HUE_COMM_TARGET_NAMES = ["Licht", "Raum", "Zone"];

function HUE_commSleep(milliseconds) {
    var currentTime = new Date().getTime();
    while (currentTime + milliseconds >= new Date().getTime()) {
    }
}

function HUE_commPad(n) {
    return (n < 10 ? "0" : "") + n;
}

function HUE_commParam(device, name) {
    return device.getParameterByName("HUE_" + name);
}

function HUE_commHex(b) {
    var hex = "0123456789abcdef";
    return hex.charAt((b >> 4) & 0x0F) + hex.charAt(b & 0x0F);
}

// 16 Byte ab pos als UUID 8-4-4-4-12; 16 Nullbytes ergeben "".
function HUE_commUuid(resp, pos) {
    var out = "";
    var empty = true;
    for (var i = 0; i < 16; i++) {
        var b = resp[pos + i];
        if (b !== 0) empty = false;
        if (i == 4 || i == 6 || i == 8 || i == 10) out += "-";
        out += HUE_commHex(b);
    }
    return empty ? "" : out;
}

// Längenbyte + UTF-8 ab pos. Gibt {text, next} zurück; 4-Byte-Zeichen (Emoji) entfallen.
function HUE_commText(resp, pos) {
    var len = resp[pos];
    var end = pos + 1 + len;
    var text = "";
    var i = pos + 1;
    while (i < end) {
        var c = resp[i];
        if (c < 0x80) {
            text += String.fromCharCode(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < end) {
            text += String.fromCharCode(((c & 0x1F) << 6) | (resp[i + 1] & 0x3F));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < end) {
            text += String.fromCharCode(((c & 0x0F) << 12) | ((resp[i + 1] & 0x3F) << 6) | (resp[i + 2] & 0x3F));
            i += 3;
        } else {
            i += 1;
        }
    }
    return { text: text, next: end };
}

// Kürzt einen Text auf maxBytes UTF-8-Byte (so zählt die ETS die Länge von Textparametern).
function HUE_commFit(text, maxBytes) {
    var bytes = 0;
    for (var i = 0; i < text.length; i++) {
        var c = text.charCodeAt(i);
        var n = c < 0x80 ? 1 : (c < 0x800 ? 2 : 3);
        if (bytes + n > maxBytes) return text.substring(0, i);
        bytes += n;
    }
    return text;
}

function HUE_commSame(a, b) {
    return ("" + a).toLowerCase() == ("" + b).toLowerCase();
}

function HUE_commClearTables(device) {
    var r;
    for (r = 1; r <= HUE_COMM_MAX_LIGHTS; r++) {
        var l = "HUECommL" + HUE_commPad(r);
        HUE_commParam(device, l + "Name").value = "";
        HUE_commParam(device, l + "Id").value = "";
        HUE_commParam(device, l + "Room").value = "";
        HUE_commParam(device, l + "RoomId").value = "";
        HUE_commParam(device, l + "Zone").value = "";
        HUE_commParam(device, l + "ZoneId").value = "";
        HUE_commParam(device, l + "Channel").value = 0;
        HUE_commParam(device, l + "Target").value = 0;
    }
    for (r = 1; r <= HUE_COMM_MAX_ACCESSORIES; r++) {
        var a = "HUECommA" + HUE_commPad(r);
        HUE_commParam(device, a + "Name").value = "";
        HUE_commParam(device, a + "Type").value = 0;
        HUE_commParam(device, a + "Id").value = "";
        HUE_commParam(device, a + "Channel").value = 0;
    }
    for (r = 1; r <= HUE_COMM_MAX_SCENES; r++) {
        var s = "HUECommS" + HUE_commPad(r);
        HUE_commParam(device, s + "Name").value = "";
        HUE_commParam(device, s + "Room").value = "";
        HUE_commParam(device, s + "Id").value = "";
        HUE_commParam(device, s + "Slot").value = 0;
    }
    HUE_commParam(device, "HUECommLightCount").value = 0;
    HUE_commParam(device, "HUECommAccessoryCount").value = 0;
    HUE_commParam(device, "HUECommSceneCount").value = 0;
}

function HUE_commFetch(device, online, progress, kind, index) {
    var resp = BASE_invokeFunctionPropertyWrapper(HUE_COMM_OBJECT, HUE_COMM_PROPERTY, [3, kind, index], device, online, progress);
    if (!resp || resp.length < 1 || resp[0] !== 0) {
        throw new Error("Hue: Ungültige Antwort beim Abholen von Eintrag " + (index + 1) + ".");
    }
    return resp;
}

// Sucht den Kanal, dessen Ziel-ID auf id zeigt. channelTypes: erlaubte Kanaltypen,
// targetType: bei Licht-Kanälen der geforderte Zieltyp (-1 = egal). 0 = keiner.
function HUE_commFindChannel(device, id, channelTypes, targetType) {
    if (id === "") return 0;
    for (var ch = 1; ch <= HUE_COMM_CHANNELS; ch++) {
        var type = HUE_commParam(device, "CH" + ch + "ChannelType");
        if (!type) break;
        var allowed = false;
        for (var i = 0; i < channelTypes.length; i++)
            if (channelTypes[i] == type.value) allowed = true;
        if (!allowed) continue;
        if (targetType >= 0 && HUE_commParam(device, "CH" + ch + "TargetType").value != targetType) continue;
        if (HUE_commSame(HUE_commParam(device, "CH" + ch + "TargetRID").value, id)) return ch;
    }
    return 0;
}

// Bestehende Zuordnungen nach dem Laden in die Spalten Kanal / Steuern als / Slot übernehmen.
// Ein Raum oder eine Zone wird nur beim ersten Licht vorbelegt, sonst stünde derselbe
// Kanal bei jedem Licht des Raums.
function HUE_commPreselect(device, lightCount, accessoryCount, sceneCount) {
    var seenGroups = "|";
    var r, ch;
    for (r = 1; r <= lightCount; r++) {
        var l = "HUECommL" + HUE_commPad(r);
        ch = HUE_commFindChannel(device, HUE_commParam(device, l + "Id").value, [1], 0);
        var target = 0;
        for (var t = 1; ch === 0 && t <= 2; t++) {
            var groupId = HUE_commParam(device, l + (t == 1 ? "RoomId" : "ZoneId")).value;
            if (groupId === "" || seenGroups.indexOf("|" + groupId + "|") >= 0) continue;
            ch = HUE_commFindChannel(device, groupId, [1], t);
            if (ch > 0) {
                target = t;
                seenGroups += groupId + "|";
            }
        }
        HUE_commParam(device, l + "Channel").value = ch;
        HUE_commParam(device, l + "Target").value = target;
    }
    for (r = 1; r <= accessoryCount; r++) {
        var a = "HUECommA" + HUE_commPad(r);
        HUE_commParam(device, a + "Channel").value = HUE_commFindChannel(device, HUE_commParam(device, a + "Id").value, [2, 3, 4, 5], -1);
    }
    for (r = 1; r <= sceneCount; r++) {
        var s = "HUECommS" + HUE_commPad(r);
        var id = HUE_commParam(device, s + "Id").value;
        var slot = 0;
        for (var k = 1; id !== "" && k <= HUE_COMM_SCENE_SLOTS; k++) {
            if (HUE_commSame(HUE_commParam(device, "HUEHueScene" + k + "RID").value, id)) {
                slot = k;
                break;
            }
        }
        HUE_commParam(device, s + "Slot").value = slot;
    }
}

function HUE_btnCommPair(device, online, progress, context) {
    var status = HUE_commParam(device, "HUECommPairStatus");
    online.connect();
    try {
        progress.setText("Hue: Starte Pairing …");
        progress.setProgress(2);
        var resp = online.invokeFunctionProperty(HUE_COMM_OBJECT, HUE_COMM_PROPERTY, [4]);
        if (!resp || resp.length < 2) {
            throw new Error("Hue: Keine Antwort vom Gerät. Ist es mit dieser Applikation programmiert?");
        }
        if (resp[0] !== 0 && resp[1] == 2) {
            status.value = "Hue-Bridge ist bereits gekoppelt.";
            progress.setProgress(100);
            progress.setText("Hue: Die Bridge ist bereits gekoppelt. Für ein neues Pairing zuerst 'Authentication zurücksetzen'.");
            return;
        }
        if (resp[0] !== 0) {
            status.value = "Pairing konnte nicht gestartet werden.";
            throw new Error("Hue: Pairing nicht gestartet. Netzwerkverbindung und Bridge-Erkennung (mDNS / IP-Adresse) prüfen.");
        }

        // Nach dem Tastendruck richtet das Gerät die Kanäle ein und antwortet dabei
        // kurzzeitig nicht; solche Aufrufe werden wiederholt.
        var waited = 0;
        while (waited < HUE_COMM_PAIR_TIMEOUT_MS) {
            if (progress.isCanceled()) {
                status.value = "Pairing abgebrochen (läuft im Gerät bis zum Ende des Zeitfensters weiter).";
                return;
            }
            HUE_commSleep(1000);
            waited += 1000;
            try {
                resp = online.invokeFunctionProperty(HUE_COMM_OBJECT, HUE_COMM_PROPERTY, [5]);
            } catch (e) {
                continue;
            }
            if (!resp || resp.length < 3 || resp[0] !== 0) continue;
            if (resp[1] == 1) {
                status.value = "Hue-Bridge gekoppelt.";
                progress.setProgress(100);
                progress.setText("Hue: Bridge gekoppelt. Jetzt 'Hue-Geräte laden' ausführen.");
                return;
            }
            if (resp[1] == 2) {
                status.value = "Pairing fehlgeschlagen: Link-Button nicht rechtzeitig gedrückt oder Bridge nicht erreichbar.";
                throw new Error("Hue: " + status.value);
            }
            progress.setProgress(2 + Math.floor(96 * waited / HUE_COMM_PAIR_TIMEOUT_MS));
            progress.setText("Hue: Jetzt den Link-Button an der Hue-Bridge drücken (noch " + resp[2] + " s) …");
        }
        status.value = "Zeitüberschreitung beim Pairing.";
        throw new Error("Hue: Das Pairing wurde nicht rechtzeitig abgeschlossen.");
    } finally {
        online.disconnect();
    }
}

function HUE_btnCommLoad(device, online, progress, context) {
    var status = HUE_commParam(device, "HUECommStatus");
    online.connect();
    try {
        progress.setText("Hue: Starte Abfrage der Hue-Bridge …");
        progress.setProgress(2);
        var resp = online.invokeFunctionProperty(HUE_COMM_OBJECT, HUE_COMM_PROPERTY, [1]);
        if (!resp || resp.length < 2) {
            throw new Error("Hue: Keine Antwort vom Gerät. Ist es mit dieser Applikation programmiert?");
        }
        if (resp[0] !== 0) {
            status.value = "Keine Verbindung zur Hue-Bridge.";
            throw new Error("Hue: Das Gerät hat keine aktive Verbindung zur Hue-Bridge. Bitte Pairing und Netzwerk prüfen.");
        }

        // Der Scan blockiert das Gerät einige Sekunden; Aufrufe in dieser Zeit können
        // ohne Antwort bleiben und werden einfach wiederholt.
        var done = null;
        var waited = 0;
        while (!done && waited < HUE_COMM_SCAN_TIMEOUT_MS) {
            if (progress.isCanceled()) return;
            HUE_commSleep(1000);
            waited += 1000;
            progress.setProgress(2 + Math.floor(38 * waited / HUE_COMM_SCAN_TIMEOUT_MS));
            progress.setText("Hue: Bridge wird abgefragt (" + Math.floor(waited / 1000) + " s) …");
            try {
                resp = online.invokeFunctionProperty(HUE_COMM_OBJECT, HUE_COMM_PROPERTY, [2]);
            } catch (e) {
                continue;
            }
            if (resp && resp.length >= 2 && resp[0] === 0 && resp[1] === 1) done = resp;
        }
        if (!done) {
            status.value = "Zeitüberschreitung bei der Abfrage der Hue-Bridge.";
            throw new Error("Hue: Die Abfrage der Hue-Bridge wurde nicht rechtzeitig fertig.");
        }
        if (done.length < 7 || done[2] !== 0) {
            status.value = done[2] == 1 ? "Keine Verbindung zur Hue-Bridge." : "Die Hue-Bridge lieferte keine Lichter.";
            throw new Error("Hue: " + status.value + " Details im WebUI unter 'Hue-Geräte laden'.");
        }

        var lightCount = done[3];
        var accessoryCount = done[4];
        var sceneCount = done[5];
        var truncated = done[6];
        var total = lightCount + accessoryCount + sceneCount;
        var fetched = 0;

        HUE_commClearTables(device);

        var r, pos, t;
        for (r = 1; r <= lightCount; r++) {
            if (progress.isCanceled()) return;
            resp = HUE_commFetch(device, online, progress, 1, r - 1);
            var l = "HUECommL" + HUE_commPad(r);
            HUE_commParam(device, l + "Id").value = HUE_commUuid(resp, 1);
            t = HUE_commText(resp, 17);
            HUE_commParam(device, l + "Name").value = t.text;
            HUE_commParam(device, l + "RoomId").value = HUE_commUuid(resp, t.next);
            t = HUE_commText(resp, t.next + 16);
            HUE_commParam(device, l + "Room").value = t.text;
            HUE_commParam(device, l + "ZoneId").value = HUE_commUuid(resp, t.next);
            t = HUE_commText(resp, t.next + 16);
            HUE_commParam(device, l + "Zone").value = t.text;
            fetched++;
            progress.setProgress(40 + Math.floor(58 * fetched / total));
            progress.setText("Hue: Licht " + r + " von " + lightCount + " …");
        }
        for (r = 1; r <= accessoryCount; r++) {
            if (progress.isCanceled()) return;
            resp = HUE_commFetch(device, online, progress, 2, r - 1);
            var a = "HUECommA" + HUE_commPad(r);
            HUE_commParam(device, a + "Type").value = resp[1];
            HUE_commParam(device, a + "Id").value = HUE_commUuid(resp, 2);
            HUE_commParam(device, a + "Name").value = HUE_commText(resp, 18).text;
            fetched++;
            progress.setProgress(40 + Math.floor(58 * fetched / total));
            progress.setText("Hue: Schalter/Sensor " + r + " von " + accessoryCount + " …");
        }
        for (r = 1; r <= sceneCount; r++) {
            if (progress.isCanceled()) return;
            resp = HUE_commFetch(device, online, progress, 3, r - 1);
            var s = "HUECommS" + HUE_commPad(r);
            HUE_commParam(device, s + "Id").value = HUE_commUuid(resp, 1);
            t = HUE_commText(resp, 17);
            HUE_commParam(device, s + "Name").value = t.text;
            HUE_commParam(device, s + "Room").value = HUE_commText(resp, t.next).text;
            fetched++;
            progress.setProgress(40 + Math.floor(58 * fetched / total));
            progress.setText("Hue: Szene " + r + " von " + sceneCount + " …");
        }

        HUE_commParam(device, "HUECommLightCount").value = lightCount;
        HUE_commParam(device, "HUECommAccessoryCount").value = accessoryCount;
        HUE_commParam(device, "HUECommSceneCount").value = sceneCount;
        HUE_commPreselect(device, lightCount, accessoryCount, sceneCount);

        var text = lightCount + " Lichter, " + accessoryCount + " Schalter/Sensoren, " + sceneCount + " Szenen geladen.";
        if (truncated) text += " Nicht alle Geräte passen in die Tabellen, Rest über das WebUI.";
        status.value = text;
        HUE_commParam(device, "HUECommResult").value = "";
        progress.setProgress(100);
        progress.setText("Hue: " + text);
    } finally {
        online.disconnect();
    }
}

// Merkt die gewünschte Zuordnung eines Kanals vor. Dieselbe Zuordnung zweimal ist kein
// Konflikt (z.B. zwei Lichter desselben Raums auf einem Kanal mit Ziel Raum).
function HUE_commAssign(device, plan, ch, type, target, id, name, label) {
    if (!HUE_commParam(device, "CH" + ch + "ChannelType")) {
        plan.errors.push(label + ": Kanal Hue " + ch + " gibt es in dieser Applikation nicht.");
    } else if (plan.chId[ch] === undefined) {
        plan.chType[ch] = type;
        plan.chTarget[ch] = target;
        plan.chId[ch] = id;
        plan.chName[ch] = name;
        plan.chLabel[ch] = label;
    } else if (plan.chType[ch] != type || plan.chTarget[ch] != target || !HUE_commSame(plan.chId[ch], id)) {
        plan.errors.push("Hue " + ch + " ist doppelt belegt: " + plan.chLabel[ch] + " und " + label + ".");
    }
}

// Offline-Knopf: bekommt kein nutzbares progress-Objekt, Rückmeldung im Übernahme-Resultat.
function HUE_btnCommApply(device, online, progress, context) {
    var result = HUE_commParam(device, "HUECommResult");
    var lines = [];
    // Je Kanal / Slot die gewünschte Zuordnung; flache Arrays, Index = Kanal bzw. Slot.
    var plan = { errors: [], chType: [], chTarget: [], chId: [], chName: [], chLabel: [] };
    var errors = plan.errors;
    var chType = plan.chType, chTarget = plan.chTarget, chId = plan.chId, chName = plan.chName, chLabel = plan.chLabel;
    var slotId = [], slotName = [];
    var r, ch, label;

    var lightCount = parseInt(HUE_commParam(device, "HUECommLightCount").value);
    var accessoryCount = parseInt(HUE_commParam(device, "HUECommAccessoryCount").value);
    var sceneCount = parseInt(HUE_commParam(device, "HUECommSceneCount").value);
    if (lightCount + accessoryCount + sceneCount === 0) {
        result.value = "Keine Hue-Geräte geladen. Bitte zuerst 'Hue-Geräte laden' ausführen.";
        return;
    }

    for (r = 1; r <= lightCount; r++) {
        var l = "HUECommL" + HUE_commPad(r);
        ch = HUE_commParam(device, l + "Channel").value;
        if (ch == 0) continue;
        var target = HUE_commParam(device, l + "Target").value;
        var name = HUE_commParam(device, l + "Name").value;
        var id = HUE_commParam(device, l + (target == 1 ? "RoomId" : (target == 2 ? "ZoneId" : "Id"))).value;
        var targetName = target == 0 ? name : HUE_commParam(device, l + (target == 1 ? "Room" : "Zone")).value;
        label = HUE_COMM_TARGET_NAMES[target] + " '" + targetName + "'";
        if (id === "") {
            errors.push("Licht '" + name + "' hat keine " + (target == 1 ? "Raum" : "Zone") + "-Zuordnung in der Hue-App.");
            continue;
        }
        HUE_commAssign(device, plan, ch, 1, target, id, targetName, label);
    }
    for (r = 1; r <= accessoryCount; r++) {
        var a = "HUECommA" + HUE_commPad(r);
        ch = HUE_commParam(device, a + "Channel").value;
        if (ch == 0) continue;
        var accName = HUE_commParam(device, a + "Name").value;
        var type = HUE_COMM_ACCESSORY_CHANNEL_TYPE[HUE_commParam(device, a + "Type").value];
        if (!type) {
            errors.push("'" + accName + "' kann keinem Hue-Kanal zugeordnet werden (Typ nicht unterstützt).");
            continue;
        }
        HUE_commAssign(device, plan, ch, type, 0, HUE_commParam(device, a + "Id").value, accName, HUE_COMM_CHANNEL_TYPE_NAMES[type] + " '" + accName + "'");
    }
    for (r = 1; r <= sceneCount; r++) {
        var s = "HUECommS" + HUE_commPad(r);
        var slot = HUE_commParam(device, s + "Slot").value;
        if (slot == 0) continue;
        var sceneId = HUE_commParam(device, s + "Id").value;
        var sceneName = HUE_commParam(device, s + "Name").value + " (" + HUE_commParam(device, s + "Room").value + ")";
        if (slotId[slot] === undefined) {
            slotId[slot] = sceneId;
            slotName[slot] = sceneName;
        } else if (!HUE_commSame(slotId[slot], sceneId)) {
            errors.push("Hue Szene " + slot + " ist doppelt belegt: '" + slotName[slot] + "' und '" + sceneName + "'.");
        }
    }

    if (errors.length > 0) {
        result.value = "Nichts übernommen:\n" + errors.join("\n");
        return;
    }

    for (ch = 1; ch <= HUE_COMM_CHANNELS; ch++) {
        if (chId[ch] === undefined) continue;
        var prefix = "CH" + ch;
        var notes = [];
        var oldType = HUE_commParam(device, prefix + "ChannelType").value;
        if (oldType != chType[ch]) {
            if (oldType != 0) notes.push("Gerätetyp " + HUE_COMM_CHANNEL_TYPE_NAMES[oldType] + " -> " + HUE_COMM_CHANNEL_TYPE_NAMES[chType[ch]]);
            HUE_commParam(device, prefix + "ChannelType").value = chType[ch];
        }
        // Kanalkopf wird sonst erst über BASE_SyncChannelType nachgezogen
        HUE_commParam(device, prefix + "ChannelTypeSelect").value = chType[ch];
        if (chType[ch] == 1) HUE_commParam(device, prefix + "TargetType").value = chTarget[ch];
        var rid = HUE_commParam(device, prefix + "TargetRID");
        if (rid.value !== "" && !HUE_commSame(rid.value, chId[ch])) notes.push("vorherige Ziel-ID ersetzt");
        rid.value = chId[ch];
        var channelName = HUE_commParam(device, prefix + "Name");
        if (channelName.value === "") {
            channelName.value = HUE_commFit(chName[ch], HUE_COMM_CHANNEL_NAME_BYTES);
            notes.push("Beschreibung gesetzt");
        }
        lines.push("Hue " + ch + ": " + chLabel[ch] + " übernommen" + (notes.length ? " (" + notes.join(", ") + ")" : "") + ".");
    }

    var scenesUsed = false;
    for (var k = 1; k <= HUE_COMM_SCENE_SLOTS; k++) {
        if (slotId[k] === undefined) continue;
        HUE_commParam(device, "HUEHueScene" + k + "RID").value = slotId[k];
        lines.push("Hue Szene " + k + ": '" + slotName[k] + "' übernommen.");
        scenesUsed = true;
    }
    if (scenesUsed && HUE_commParam(device, "HUEScenesEnable").value == 0) {
        HUE_commParam(device, "HUEScenesEnable").value = 1;
        lines.push("Hue Szenen aktiviert.");
    }

    result.value = lines.length ? lines.join("\n") : "Keine Zuordnung gewählt.";
}
