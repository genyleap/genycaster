#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
errors: list[str] = []

def read(rel: str) -> str:
    return (root / rel).read_text(encoding='utf-8')

app_cmake = read('src/application/CMakeLists.txt')
domain = read('src/domain/rich_document.cppm')
rich = read('src/application/rich_format.cppm')
rich_impl = read('src/application/rich_format.cpp')
syntax = read('src/application/syntax.cpp')
protocol = read('src/protocol/message.cpp')
composer = read('ui/components/composer/ComposerDrawer.qml')
renderer = read('ui/components/content/RichCastRenderer.qml')
cast_item = read('ui/components/feed/CastItem.qml')
detail = read('ui/components/feed/DetailSurface.qml')
insert_panel = read('ui/components/composer/ComposerInsertPanel.qml')
rich_controller = read('src/presentation/qt/rich_composer_controller.cpp')

for token in ('add_library(farcaster_application_rich)', 'rich_format.cppm', 'syntax.cppm'):
    if token not in app_cmake:
        errors.append(f'Independent rich publishing target is missing {token}.')
for token in ('schemaVersion', 'RichCastDocument', 'CodeBlock', 'CastReferenceBlock', 'MediaBlock'):
    if token not in domain:
        errors.append(f'Rich AST contract is missing {token}.')
for token in ('FarcasterSafeSerializer', 'MarkdownImporter', 'MarkdownExporter', 'MentionPlacement'):
    if token not in rich:
        errors.append(f'Rich format application contract is missing {token}.')
if 'bytePosition = static_cast<std::uint32_t>(output.size())' not in rich_impl:
    errors.append('Mention positions must be derived after final UTF-8 safe-text layout.')
if 'input.text.size() > maximumCastTextBytes' not in protocol or 'CastType::LongCast' not in protocol:
    errors.append('Protocol codec must validate the current LONG_CAST publication envelope.')
for lang in ('cpp','c','php','javascript','typescript','rust','qml','json','html','css','scss','bash','curl','sql','python','java','csharp','swift','objective-c','cmake','yaml','xml','markdown','dockerfile','nginx','http','graphql'):
    if f'{{"{lang}"' not in syntax:
        errors.append(f'Syntax registry is missing {lang}.')
for token in ('RichTextEditor', 'FarcasterPreview', 'ComposerInsertPanel', 'editorMode === "markdown"', 'richComposer.safeText'):
    if token not in composer:
        errors.append(f'Composer drawer is missing {token}.')
for token in ('applyLink', 'resolveMention', 'insertEmbed', 'mediaFileRequested'):
    if token not in insert_panel:
        errors.append(f'Composer insert panel is missing {token}.')
if 'mediaUpload.beginUpload' not in composer:
    errors.append('Composer media-upload path is missing mediaUpload.beginUpload.')
if 'richComposer.insertMedia' not in composer and 'richComposer.insertManagedMedia' not in composer:
    errors.append('Composer media-upload path must insert uploaded media through the rich composer.')
if 'richComposer.richBlocksForCast' not in renderer:
    errors.append('Central rich renderer must resolve optional sidecar metadata.')
if 'RichCastRenderer' not in cast_item or 'RichCastRenderer' not in detail:
    errors.append('Feed/detail cast paths must share RichCastRenderer.')
if (root / 'ui/components/BackdropBlur.qml').exists() or 'BackdropBlur' in composer:
    errors.append('Composer backdrop blur must remain removed; the dark drawer surface is sufficient.')
if 'lhs.fid == rhs.fid' in rich_controller or 'rhs.fid == lhs.fid' in rich_controller:
    errors.append('Presentation must not depend on std::optional<Fid> equality under Clang module builds.')
if 'sameOptionalFid(lhs.fid, rhs.fid)' not in rich_controller:
    errors.append('Rich composer must compare optional FIDs through the Clang-safe strong-type helper.')

if errors:
    print('R7 release contract: FAILED')
    for error in errors:
        print(' -', error)
    sys.exit(1)
print('R7 release contract: PASS')
