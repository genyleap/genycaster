import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

const shell=readFileSync('ui/components/shell/AppShell.qml','utf8');
const begin=shell.indexOf('function distributeRecoveryWords(start, pasted) {');
const end=shell.indexOf('\n        onOpened:',begin);
assert.ok(begin>=0 && end>begin);
const implementation=shell.slice(begin,end).trim();

function createHarness(){
    let focusCalls=0;
    let reentered=false;
    let distribute;
    const fields=Array.from({length:24},(_,index)=>({
        value:'',
        get word(){return this.value;},
        set word(next){
            this.value=next;
            // Programmatic changes notify QML textChanged synchronously.
            // It must not recursively distribute the same input again.
            if(index===0 && !reentered){
                reentered=true;
                distribute(0,'abandon able');
            }
        },
        focusWord(){focusCalls++;}
    }));
    const context={
        distributingRecoveryWords:false,
        recoveryRevision:0,
        nativeSignerWordGrid:{itemAt:i=>fields[i]||null}
    };
    distribute=vm.runInNewContext('('+implementation+')',context);
    return {fields,context,distribute,get focusCalls(){return focusCalls;}};
}

test('native QML accepts a full 24-word clipboard export from the first or a later field',()=>{
    const h=createHarness();
    const vector=[...Array(23).fill('abandon'),'art'];
    h.distribute(0,vector.join(' '));
    assert.deepEqual(h.fields.map(f=>f.word),vector);
    assert.equal(h.context.distributingRecoveryWords,false);
    assert.equal(h.context.recoveryRevision,1);
    assert.equal(h.focusCalls,0,'pasting must never advance focus');

    const other=createHarness();
    other.distribute(3,vector.map((word,i)=>String(i+1).padStart(2,'0')+'. '+word).join('\n'));
    assert.deepEqual(other.fields.map(f=>f.word),vector);
    assert.equal(other.focusCalls,0);
});

test('partial native clipboard entry stays in the chosen field instead of jumping to 04',()=>{
    const h=createHarness();
    h.distribute(0,'abandon ability able');
    assert.deepEqual(h.fields.slice(0,3).map(f=>f.word),['abandon','ability','able']);
    assert.equal(h.fields[3].word,'');
    assert.equal(h.focusCalls,0);
    const overflow=createHarness();
    overflow.distribute(23,'abandon ability able');
    assert.equal(overflow.fields[23].word,'');
    assert.equal(overflow.focusCalls,0);
});

test('native secret editor defers paste processing until Qt settles the edit',()=>{
    const field=shell.slice(shell.indexOf('objectName: "nativeSignerRecoveryWord"'),
      shell.indexOf('            ActionButton {',shell.indexOf('objectName: "nativeSignerRecoveryWord"')));
    assert.match(field,/Qt\.callLater\(function\(\)/);
    assert.match(field,/interactionAuthorizationDialog\.distributingRecoveryWords/);
    assert.match(field,/distributeRecoveryWords\(/);
    assert.doesNotMatch(field,/onEditingFinished/);
    assert.match(field,/Keys\.onPressed:/);
});
