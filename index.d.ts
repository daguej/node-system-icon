export let ICON_SIZE_EXTRA_SMALL: number;
export let ICON_SIZE_SMALL: number;
export let ICON_SIZE_MEDIUM: number;
export let ICON_SIZE_LARGE: number;
export let ICON_SIZE_EXTRA_LARGE: number;
/**
 * @overload
 * @param {string} extension
 * @param {number} size
 * @param {IconForExtensionCallback} cb
 * @returns {void}
 */
export function getIconForExtension(extension: string, size: number, cb: (err: NodeJS.ErrnoException | null, uint8?: Uint8Array<ArrayBufferLike>) => void): void;
/**
 * @overload
 * @param {string} extension
 * @param {number} size
 * @returns {Promise<Uint8Array<ArrayBufferLike>>}
 */
export function getIconForExtension(extension: string, size: number): Promise<Uint8Array<ArrayBufferLike>>;
/**
 * @overload
 * @param {string} filePath
 * @param {number} size
 * @param {IconForPathCallback} cb
 * @returns {void}
 */
export function getIconForPath(filePath: string, size: number, cb: (err: NodeJS.ErrnoException | null, uint8?: Uint8Array<ArrayBufferLike>) => void): void;
/**
 * @overload
 * @param {string} filePath
 * @param {number} size
 * @returns {Promise<Uint8Array<ArrayBufferLike>>}
 */
export function getIconForPath(filePath: string, size: number): Promise<Uint8Array<ArrayBufferLike>>;
/**
 * @overload
 * @param {number} pid
 * @param {number} size
 * @param {IconForProcessCallback} cb
 * @returns {void}
 */
export function getIconForProcess(pid: number, size: number, cb: (err: NodeJS.ErrnoException | null, uint8?: Uint8Array<ArrayBufferLike>) => void): void;
/**
 * @overload
 * @param {number} pid
 * @param {number} size
 * @returns {Promise<Uint8Array<ArrayBufferLike>>}
 */
export function getIconForProcess(pid: number, size: number): Promise<Uint8Array<ArrayBufferLike>>;
