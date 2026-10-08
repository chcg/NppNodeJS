const npp = require('#menu-helper').promises;

const timer = setInterval(() => {
  console.log(`${new Date().toLocaleTimeString()} — Waiting for user confirmation`);
}, 1000);

(async () => {
  try {
    await npp.alert('Watch the Output Pane update while this alert is open.');
  } finally {
    clearInterval(timer);
  }

  console.log('The user closed the alert.');
})();
