'use strict';

// The addon fails to load when it wasn't built for this Node version or, on
// Linux, when a library it links against is missing. Requiring this module
// still succeeds then, and each call reports the error instead.
/** @type {any} */
let addon;
/** @type {Error|undefined} */
let loadError;
try {
  addon = require('bindings')('addon');
} catch (err) {
  loadError = /** @type {Error} */ (err);
  // The values of the native IconSize enum.
  addon = {
    ICON_SIZE_EXTRA_SMALL: 0,
    ICON_SIZE_SMALL: 1,
    ICON_SIZE_MEDIUM: 2,
    ICON_SIZE_LARGE: 3,
    ICON_SIZE_EXTRA_LARGE: 4
  };
}

/**
 * @param {string} name
 * @param {string|number} target
 * @param {number} size
 * @param {(
 *   err: NodeJS.ErrnoException|null,
 *   uint8?: Uint8Array<ArrayBufferLike>
 * ) => void} [cb]
 * @returns {void|Promise<Uint8Array<ArrayBufferLike>>}
 */
function getIcon (name, target, size, cb) {
  if (cb) {
    if (loadError) {
      const err = loadError;
      process.nextTick(() => cb(err));
      return;
    }
    return addon[name](target, size, cb);
  }

  return new Promise((resolve, reject) => {
    if (loadError) {
      reject(loadError);
      return;
    }
    addon[name](
      target,
      size,
      /**
       * @param {Error|null} err
       * @param {Uint8Array<ArrayBufferLike>} [result]
       */
      (err, result) => {
        if (err) {
          reject(err);
        } else {
          resolve(/** @type {Uint8Array<ArrayBufferLike>} */ (result));
        }
      });
  });
}

module.exports = {
  // Add constants
  ICON_SIZE_EXTRA_SMALL: /** @type {number} */ (addon.ICON_SIZE_EXTRA_SMALL),
  ICON_SIZE_SMALL: /** @type {number} */ (addon.ICON_SIZE_SMALL),
  ICON_SIZE_MEDIUM: /** @type {number} */ (addon.ICON_SIZE_MEDIUM),
  ICON_SIZE_LARGE: /** @type {number} */ (addon.ICON_SIZE_LARGE),
  ICON_SIZE_EXTRA_LARGE: /** @type {number} */ (addon.ICON_SIZE_EXTRA_LARGE),

  /**
   * @typedef {(
   *   err: NodeJS.ErrnoException|null,
   *   uint8?: Uint8Array<ArrayBufferLike>
   * ) => void} IconForExtensionCallback
   */

  /**
   * @overload
   * @param {string} extension
   * @param {number} size
   * @param {IconForExtensionCallback} cb
   * @returns {void}
   */
  /**
   * @overload
   * @param {string} extension
   * @param {number} size
   * @returns {Promise<Uint8Array<ArrayBufferLike>>}
   */
  /**
   * @param {string} extension
   * @param {number} size
   * @param {IconForExtensionCallback} [cb]
   * @returns {void|Promise<Uint8Array<ArrayBufferLike>>}
   */
  getIconForExtension (extension, size, cb) {
    return getIcon('getIconForExtension', extension, size, cb);
  },

  /**
   * @typedef {(
   *   err: NodeJS.ErrnoException|null,
   *   uint8?: Uint8Array<ArrayBufferLike>
   * ) => void} IconForPathCallback
   */

  /**
   * @overload
   * @param {string} filePath
   * @param {number} size
   * @param {IconForPathCallback} cb
   * @returns {void}
   */
  /**
   * @overload
   * @param {string} filePath
   * @param {number} size
   * @returns {Promise<Uint8Array<ArrayBufferLike>>}
   */
  /**
   * @param {string} filePath
   * @param {number} size
   * @param {IconForPathCallback} [cb]
   * @returns {void|Promise<Uint8Array<ArrayBufferLike>>}
   */
  getIconForPath (filePath, size, cb) {
    return getIcon('getIconForPath', filePath, size, cb);
  },

  /**
   * @typedef {(
   *   err: NodeJS.ErrnoException|null,
   *   uint8?: Uint8Array<ArrayBufferLike>
   * ) => void} IconForProcessCallback
   */

  /**
   * @overload
   * @param {number} pid
   * @param {number} size
   * @param {IconForProcessCallback} cb
   * @returns {void}
   */
  /**
   * @overload
   * @param {number} pid
   * @param {number} size
   * @returns {Promise<Uint8Array<ArrayBufferLike>>}
   */
  /**
   * @param {number} pid
   * @param {number} size
   * @param {IconForProcessCallback} [cb]
   * @returns {void|Promise<Uint8Array<ArrayBufferLike>>}
   */
  getIconForProcess (pid, size, cb) {
    return getIcon('getIconForProcess', pid, size, cb);
  }
};
