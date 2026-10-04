import npp from '#menu-helper';

let value = await npp.confirm('confirm(message):');
console.log('First result:', value);

value = await npp.confirm('confirm(message, title):', 'Custom Title');
console.log('Second result:', value);
