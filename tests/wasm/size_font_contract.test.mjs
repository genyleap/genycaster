import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
const read=(path)=>readFileSync(new URL("../../"+path,import.meta.url),"utf8");

test("WASM fonts retain full CJK coverage while removing unneeded TTC faces",()=>{
 const cmake=read("ui/CMakeLists.txt");
 const optimizer=read("tools/optimize_cjk_collection.py");
 const main=read("ui/mobile/main.cpp");
 assert.match(cmake,/GENYCASTER_OPTIMIZE_CJK_COLLECTION/);
 assert.match(cmake,/GENYCASTER_CJK_FONTTOOLS_PYTHON/);
 assert.match(cmake,/tools\/optimize_cjk_collection.py/);
 assert.match(optimizer,/FAMILIES = \("Noto Sans CJK JP", "Noto Sans CJK KR", "Noto Sans CJK SC"\)/);
 assert.match(optimizer,/original_cmaps/);
 assert.match(optimizer,/frozenset\(font.getBestCmap\(\)\)/);
 assert.match(optimizer,/optimized collection lost Unicode glyph coverage/);
 assert.match(optimizer,/SOURCE_SHA256/);
 assert.match(main,/bundledJapaneseFontFamily = chooseCjkFamily\(QStringLiteral\("JP"\)\)/);
 assert.match(main,/bundledKoreanFontFamily = chooseCjkFamily\(QStringLiteral\("KR"\)\)/);
 assert.match(main,/bundledChineseFontFamily = chooseCjkFamily\(QStringLiteral\("SC"\)\)/);
});
test("no font glyph is discarded by simplistic fixed-size UI language subset",()=>{
 const optimizer=read("tools/optimize_cjk_collection.py");
 assert.doesNotMatch(optimizer,/fontTools\.subset|unicodes.*set|glyph_names=/);
 assert.match(optimizer,/collection\.fonts = selected/);
 assert.match(optimizer,/if len\(cmap\) < 44000:/);
});