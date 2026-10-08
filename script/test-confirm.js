const npp = require('#menu-helper');

let value = npp.confirm('confirm(message):');
console.log('First result:', value);

value = npp.confirm('confirm(message, title):', 'Custom Title');
console.log('Second result:', value);
