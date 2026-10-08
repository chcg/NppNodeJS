const npp = require('#menu-helper');

const original = npp.getLine();
console.log('Current line:', original);

npp.setLine(original + ' [setLine test]');
console.log('After replacementCurrent line:', npp.getLine());
