const npp = require('#menu-helper');

(async () => {
  const text = await npp.getText();
  console.log('Full text length:', text.length);
  console.log('First 100 characters:', text.slice(0, 100));

  await npp.setText(text + '\n[setText test]');
  console.log('Full text replaced. New length:', (await npp.getText()).length);
})();
