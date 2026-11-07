import {readFileSync} from 'node:fs';
// The physical source registry does not change public Qt QML type aliases.
const physical=Object.freeze(JSON.parse(readFileSync(new URL('../../ui/components/source_paths.json',import.meta.url),'utf8')));
export function componentPath(name) {
 const hasSuffix=name.endsWith('.qml');
 const source=physical[hasSuffix?name:name+'.qml'];
 if(!source) throw new Error('Unknown QML component source: '+name);
 return hasSuffix?source:source.slice(0,-4);
}
