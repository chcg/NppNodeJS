const npp = require('#menu-helper');

(async () => {
  console.log('Current file:', await npp.getFileName());
  console.log('Current cursor:', await npp.getCursor());
})();
