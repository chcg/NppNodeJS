const npp = require('#menu-helper');

(async () => {
  const original = await npp.getLine();
  console.log('Current line:', original);

  await npp.setLine(original + ' [setLine test]');
  console.log('After replacementCurrent line:', await npp.getLine());
})();
