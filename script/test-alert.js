const npp = require('#menu-helper');

npp.alert('Use the menu.json item title.');
console.log('The default-title alert has been closed.');

npp.alert('This alert uses a custom window title.', 'Custom Title');
console.log('The custom-title alert has been closed; Node.js continues running.');
