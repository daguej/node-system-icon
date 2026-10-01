# system-icon2

An updated fork of [system-icon](https://github.com/mtojo/node-system-icon).

Get associated file/folder icon for Node.js.

## Installation

```bash
$ npm install --save system-icon2
```

### Linux build prerequisites

On Linux the addon links against GIO and gdk-pixbuf (no GTK required), so their
development headers must be installed before `npm install`:

```bash
# Debian/Ubuntu
$ sudo apt-get install libglib2.0-dev libgdk-pixbuf-2.0-dev

# Fedora
$ sudo dnf install glib2-devel gdk-pixbuf2-devel
```

Most icon themes ship SVG icons, which need gdk-pixbuf's SVG loader at runtime
(`librsvg2-common` on Debian/Ubuntu, `librsvg2` on Fedora); desktop systems
almost always have it already.

If runtime dependencies are missing, the module will still load correctly, but
every call to `getIcon…` will throw.

### For Electron Apps

If you're using this package in an Electron application, you need to rebuild it for Electron's Node.js version:

```bash
# Install electron-rebuild as a dev dependency
$ npm install --save-dev electron-rebuild

# Rebuild native modules for Electron
$ npx electron-rebuild
```

Or add this to your package.json scripts:

```json
{
  "scripts": {
    "postinstall": "electron-rebuild"
  }
}
```

Alternatively, if using pnpm:

```bash
$ pnpm install --save-dev electron-rebuild
$ pnpm exec electron-rebuild
```

## Supported platforms

* macOS 10.6 or later
* Windows Vista or later
* Linux (any freedesktop.org-compliant icon theme)

On Linux, icons are looked up in the current icon theme, read from GNOME's
`org.gnome.desktop.interface icon-theme` setting, KDE's `kdeglobals`, or GTK's
`settings.ini`, then falling back to Adwaita and hicolor. Passing a `.desktop`
file to `getIconForPath` returns the application's icon.

## Usage

Get icon for file or folder path:

```js
const {writeFileSync} = require('fs');
const {
  getIconForPath,
  ICON_SIZE_MEDIUM
} = require('system-icon2');

getIconForPath("/path/to/file_or_folder", ICON_SIZE_MEDIUM, (err, result) => {
  if (err) {
    console.error(err);
  } else {
    writeFileSync("icon.png", result);
  }
});
```

Get icon for file extension:

```js
const {writeFileSync} = require('fs');
const {
  getIconForExtension,
  ICON_SIZE_MEDIUM
} = require('system-icon2');

getIconForExtension(".ext", ICON_SIZE_MEDIUM, (err, result) => {
  if (err) {
    console.error(err);
  } else {
    writeFileSync("icon.png", result);
  }
});
```

Get icon for a running process:

```js
const {writeFileSync} = require('fs');
const {
  getIconForProcess,
  ICON_SIZE_MEDIUM
} = require('system-icon2');

getIconForProcess(process.pid, ICON_SIZE_MEDIUM).then((result) => {
  writeFileSync("icon.png", result);
});
```

## API

### Constants

#### Size constants

The correspondence between the size constants and the icon size actually obtainable on each platform is as follows:

| Constant                | Windows | macOS   | Linux   |
| ----------------------- | ------- | ------- | ------- |
| `ICON_SIZE_EXTRA_SMALL` |  16x16  |  16x16  |  16x16  |
| `ICON_SIZE_SMALL`       |  32x32  |  32x32  |  32x32  |
| `ICON_SIZE_MEDIUM`      |  64x64  |  64x64  |  64x64  |
| `ICON_SIZE_LARGE`       | 256x256 | 256x256 | 256x256 |
| `ICON_SIZE_EXTRA_LARGE` | 256x256 | 512x512 | 512x512 |

### Functions

#### getIconForPath(path, size, callback)

Gets associated icon for file or folder path, and returns it in the PNG format.

#### getIconForExtension(extension, size, callback)

Gets associated icon for file extension, and returns it in the PNG format.

#### getIconForProcess(pid, size, callback)

Gets the icon of the application a running process belongs to, and returns it
in the PNG format. A process with no icon of its own, such as one that isn't
part of an installed application, fails with "Failed to load icon" rather than
getting a generic icon.

* **Windows:** the icon of the process's executable, if it has any.
* **macOS:** the icon of the running application, or else of the innermost
  `.app` bundle containing the executable.
* **Linux:** the icon from the application's `.desktop` file. It is found from
  the desktop file the process was launched from, the Flatpak or Snap the
  process runs in, or by matching the executable (or, for interpreters such as
  Python, the script) against the `Exec=` line of installed applications, or
  against the program a launcher script on that line runs from beside it, as
  Chrome's.
  Processes owned by other users can only be partly inspected, so some of them
  may have no icon.

All functions return a Promise if no callback is given.

If the native addon can't be loaded, for example because it was built for a
different Node.js version or, on Linux, a library it needs isn't installed,
`require('system-icon2')` still succeeds and every call fails with the load
error (`code: 'ERR_DLOPEN_FAILED'`).

## Troubleshooting

### Electron: "was compiled against a different Node.js version"

If calls fail with an error like:
```
The module was compiled against a different Node.js version using
NODE_MODULE_VERSION 127. This version of Node.js requires
NODE_MODULE_VERSION 140.
```

This happens because the native addon was compiled for regular Node.js, but Electron uses its own Node.js version. Solution:

1. Install `electron-rebuild`:
   ```bash
   npm install --save-dev electron-rebuild
   # or
   pnpm install --save-dev electron-rebuild
   ```

2. Rebuild the native modules for Electron:
   ```bash
   npx electron-rebuild
   # or
   pnpm exec electron-rebuild
   ```

3. If the issue persists, try a clean rebuild:
   ```bash
   # Remove node_modules and rebuild
   rm -rf node_modules package-lock.json
   npm install
   npx electron-rebuild

   # For pnpm users
   rm -rf node_modules pnpm-lock.yaml
   pnpm install
   pnpm exec electron-rebuild
   ```

4. You can also add `electron-rebuild` to your postinstall script:
   ```json
   {
     "scripts": {
       "postinstall": "electron-rebuild"
     }
   }
   ```

### Electron: "non context-aware native addons has been disabled"

This package is context-aware and should work with Electron. If you still see this error, ensure you're using the latest version (0.2.0+).

## License

MIT
