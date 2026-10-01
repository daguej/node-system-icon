const system = require("./index");
const {existsSync} = require('fs');
const {writeFile} = require('fs/promises');

// const {getIconForPath, ICON_SIZE_MEDIUM} = require('system-icon2');


// system.getIconForPath("/Applications/Brave Browser.app", system.ICON_SIZE_MEDIUM, (err, result) => {
//     if (err) {
//         console.error(err);
//     } else {
//         writeFileSync("icon.png", result);
//     }
// });

const PNG_SIGNATURE = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);

/**
 * @param {Uint8Array|undefined} result
 * @param {string} label
 */
function assertPng (result, label) {
  if (!result || !Buffer.from(result).subarray(0, 8).equals(PNG_SIGNATURE)) {
    throw new Error(`${label}: result is not a PNG`);
  }
}

/**
 * @param {Promise<unknown>} promise
 * @param {string} label
 */
async function assertNoIcon (promise, label) {
  const icon = await promise.then(() => true, () => false);
  if (icon) {
    throw new Error(`${label}: expected no icon`);
  }
}

function getTestPath () {
  if (process.platform === 'darwin' && existsSync("/Applications/Brave Browser.app")) {
    return "/Applications/Brave Browser.app";
  }
  return __dirname;
}

(async () => {
const result = await system.getIconForPath(getTestPath(), system.ICON_SIZE_MEDIUM);
assertPng(result, 'getIconForPath');
await writeFile("icon.png", /** @type {Uint8Array} */ (result));

const result2 = await system.getIconForExtension(".xml", system.ICON_SIZE_MEDIUM);
assertPng(result2, 'getIconForExtension');
await writeFile("icon-ext.png", /** @type {Uint8Array} */ (result2));

// Node has an icon of its own only on Windows; elsewhere it isn't part of an
// installed application, so it has none.
if (process.platform === 'win32') {
  const result3 = await system.getIconForProcess(process.pid, system.ICON_SIZE_MEDIUM);
  assertPng(result3, 'getIconForProcess');
  await writeFile("icon-process.png", /** @type {Uint8Array} */ (result3));
} else {
  await assertNoIcon(system.getIconForProcess(process.pid, system.ICON_SIZE_MEDIUM), 'getIconForProcess (node)');
}
await assertNoIcon(system.getIconForProcess(0x7fffffff, system.ICON_SIZE_MEDIUM), 'getIconForProcess (no such pid)');

console.log('ok');
})().catch((err) => {
  console.error(err);
  process.exitCode = 1;
});
