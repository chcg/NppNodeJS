const npp = require('#menu-helper');

(async () => {
  let value = await npp.prompt('prompt(message):');
  console.log('First result:', value);

  value = await npp.prompt('prompt(message, defaultValue):', 'Default text');
  console.log('Second result:', value);

  value = await npp.prompt('prompt(message, defaultValue, title):', 'Default value', 'Custom Title');
  console.log('Third result:', value);

  value = await npp.prompt('Cancel test:', 'Click Cancel to test.');
  console.log('Cancel result:', value);
})();
