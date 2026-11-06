import test from "node:test";
import assert from "node:assert/strict";
import {createHash} from "node:crypto";
import {readFileSync} from "node:fs";

// Runtime binding functions live in a separate Qt composition unit to keep
// GCC C++26 module compilation bounded; test both halves as one read pipeline.
function readGenycasterMain() {
  return readFileSync(new URL('../../ui/app/main.cpp', import.meta.url), 'utf8') +
    '\n' + readFileSync(new URL('../../ui/app/runtime_ui_bindings.cpp', import.meta.url), 'utf8');
}

const read = path => readFileSync(new URL("../../" + path, import.meta.url));
const fallback = read("ui/assets/fonts/GenycasterEmojiFallback.ttf");

function unicodeCoverage(bytes) {
    const numTables = bytes.readUInt16BE(4);
    let cmapOffset = -1;
    for (let i = 0; i < numTables; i++) {
        const p = 12 + i * 16;
        if (bytes.toString("ascii", p, p + 4) === "cmap") {
            cmapOffset = bytes.readUInt32BE(p + 8);
            break;
        }
    }
    assert.ok(cmapOffset >= 0, "TTF contains a cmap table");
    const records = bytes.readUInt16BE(cmapOffset + 2);
    const covered = [];
    for (let i = 0; i < records; i++) {
        const p = cmapOffset + 4 + i * 8;
        const pos = cmapOffset + bytes.readUInt32BE(p + 4);
        if (bytes.readUInt16BE(pos) !== 12) continue;
        const groups = bytes.readUInt32BE(pos + 12);
        for (let j = 0; j < groups; j++) {
            const g = pos + 16 + j * 12;
            covered.push([bytes.readUInt32BE(g), bytes.readUInt32BE(g + 4)]);
        }
        break;
    }
    assert.ok(covered.length > 0, "Supplementary-plane Unicode cmap exists");
    return codepoint => covered.some(([first,last]) => first <= codepoint && codepoint <= last);
}

test("monochrome emoji fallback is under 1 MiB and its bytes are pinned", () => {
    assert.ok(fallback.length < 1024 * 1024);
    assert.equal(fallback.toString("hex", 0, 4), "00010000");
    assert.equal(createHash("sha256").update(fallback).digest("hex"),
        "0e9754e6501fd98dbc33d42fc73d4fe1ac1d49ce88654d826b60d0ab5c945530");
});

test("lightweight fallback includes common emoji and DNA glyph", () => {
    const covers = unicodeCoverage(fallback);
    for (const code of [0x1f9ec, 0x1f600, 0x1f525, 0x1f44d, 0x1f680, 0x1f30d])
        assert.equal(covers(code), true, "Missing common emoji U+" + code.toString(16));
});

test("WASM retains pinned lightweight fallback and bundles full color emoji without a CDN", () => {
    const cmake = read("ui/CMakeLists.txt").toString("utf8");
    const main = readGenycasterMain().toString("utf8");
    assert.match(cmake, /qt_add_resources\(genycaster genycaster_emoji_fallback/);
    assert.match(cmake, /_emojiFallbackSize GREATER 1048576/);
    assert.match(cmake, /_emojiFallbackHash STREQUAL/);
    assert.match(main, /GenycasterEmojiFallback\.ttf/);
    assert.match(cmake, /GenycasterColorEmoji\.ttf/);
    assert.doesNotMatch(cmake, /wasm_emoji_font|GENYCASTER_WASM_EMOJI_FONT_PATH/);
    assert.doesNotMatch(main, /NotoColorEmoji-wasm\.ttf/);
    assert.match(cmake, /set\(_genycasterLargeResources BIG_RESOURCES\)/);
    assert.match(cmake, /genycaster_emoji_fallback\s+\$\{_genycasterLargeResources\}/);
    assert.match(cmake, /genycaster_script_fonts\s+\$\{_genycasterLargeResources\}/);
});

test("WASM registers a pinned complete color emoji font with CBDT, CBLC, cmap and GSUB",()=>{
    const bytes=read("ui/assets/fonts/GenycasterColorEmoji.ttf");
    const digest=createHash("sha256").update(bytes).digest("hex");
    assert.equal(digest,
        "15671215ab769fdc7162a045d56fd7d7e477c51b04e6b3c761d914d8fdd6cc44");
    assert.ok(bytes.length>9000000 && bytes.length<16000000);
    const tables=Array.from({length:bytes.readUInt16BE(4)},(_,i)=>
        bytes.toString("ascii",12+i*16,16+i*16));
    for (const table of ["CBDT","CBLC","GSUB","cmap"])assert.ok(tables.includes(table),table);
    const covers=unicodeCoverage(bytes);
    for(const code of [0x1f9ec,0x1f600,0x1f525,0x1f44d,0x1f680,0x1f3fd,0x1f1fa])
        assert.ok(covers(code),"Missing color emoji U+"+code.toString(16));
    const cmake=read("ui/CMakeLists.txt").toString("utf8");
    const main=readGenycasterMain();
    assert.match(cmake,/GenycasterColorEmoji\.ttf/);
    assert.match(cmake,/_colorEmojiHash STREQUAL/);
    assert.match(main,/QFontDatabase::setApplicationEmojiFontFamilies/);
    assert.match(main,/QFontDatabase::addApplicationEmojiFontFamily/);
    assert.match(main,/GenycasterColorEmoji\.ttf/);
});
