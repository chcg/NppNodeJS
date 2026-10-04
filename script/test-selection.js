const npp = require('#menu-helper');

(async () => {
  console.log('Has selection:', await npp.hasSelection());
  console.log('Selected text:', await npp.getSelection());

  await npp.setSelection('[setSelection test]');
  console.log('After replacementHas selection:', await npp.hasSelection());
  console.log('After replacementSelected text:', await npp.getSelection());
})();
