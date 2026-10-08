const npp = require('#menu-helper');

const text = npp.getText();
console.log('Full text length:', text.length);
console.log('First 100 characters:', text.slice(0, 100));

npp.setText(text + '\n[setText test]');
console.log('Full text replaced. New length:', npp.getText().length);
