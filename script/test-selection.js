const npp = require('#menu-helper');

console.log('Has selection:', npp.hasSelection());
console.log('Selected text:', npp.getSelection());

npp.setSelection('[setSelection test]');
console.log('After replacementHas selection:', npp.hasSelection());
console.log('After replacementSelected text:', npp.getSelection());
