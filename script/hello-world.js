console.log('hello from stdout');
console.error('hello from stderr');
console.log('Current test: Output Pane should display stdout / stderr in real time.');

let count = 0;
const timer = setInterval(() => {
  console.log(new Date().toLocaleString('en-US', {
    hour12: false
  }));
  count++;
  if (count >= 5) {
    clearInterval(timer);
  }
}, 1000);
