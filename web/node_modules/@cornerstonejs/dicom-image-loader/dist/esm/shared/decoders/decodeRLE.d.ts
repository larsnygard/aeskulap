import type { ByteArray } from 'dicom-parser';
import type { Types } from '@cornerstonejs/core';
declare function decodeRLE(imageFrame: Types.IImageFrame, pixelData: ByteArray): Promise<Types.IImageFrame>;
export declare function unpackOneBitPlanar(packed: Uint8Array, imageFrame: Types.IImageFrame, frameSize: number): Uint8Array;
export default decodeRLE;
