import npp from '#menu-helper';

await npp.alert('Use the menu.json item title.');
console.log('The default-title alert has been closed.');

await npp.alert('This alert uses a custom window title.', 'Custom Title');
console.log('The custom-title alert has been closed; Node.js continues running.');
