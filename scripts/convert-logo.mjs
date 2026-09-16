#!/usr/bin/env node
/**
 * Convert image to C header byte array for firmware logo.
 *
 * Usage:
 *   node convert-logo.mjs <input_image> [output_header]
 *   node convert-logo.mjs logo.png
 *   node convert-logo.mjs logo.png --invert --threshold 100
 *   node convert-logo.mjs ../images/localsend-logo.png ../src/images/LocalsendLogo.h --size 192 --name LocalsendLogo --rotate 0
 */

import sharp from "sharp";
import fs from "node:fs";
import path from "node:path";
import { parseArgs } from "node:util";

const DEFAULT_SIZE = 384;
const DEFAULT_NAME = "PapyrixLogo";

async function convertToLogo(inputPath, outputPath, options = {}) {
  const size = options.size ?? DEFAULT_SIZE;
  const name = options.name ?? DEFAULT_NAME;
  const invert = options.invert ?? false;
  const threshold = options.threshold ?? 128;
  const rotate = options.rotate ?? 270;

  if (!Number.isInteger(size) || size < 8 || size % 8 !== 0) {
    throw new Error("Size must be an integer multiple of 8, at least 8");
  }
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(name)) {
    throw new Error("Name must be a C identifier");
  }
  if (![0, 90, 180, 270].includes(rotate)) {
    throw new Error("Rotate must be 0, 90, 180, or 270");
  }

  const metadata = await sharp(inputPath).metadata();
  const srcRatio = metadata.width / metadata.height;

  let newWidth;
  let newHeight;
  if (srcRatio > 1) {
    newWidth = size;
    newHeight = Math.max(1, Math.round(size / srcRatio));
  } else {
    newHeight = size;
    newWidth = Math.max(1, Math.round(size * srcRatio));
  }

  const { data: resized, info } = await sharp(inputPath)
    .flatten({ background: { r: 255, g: 255, b: 255 } })
    .removeAlpha()
    .resize(newWidth, newHeight, { fit: "fill" })
    .grayscale()
    .raw()
    .toBuffer({ resolveWithObject: true });

  if (info.channels !== 1) {
    throw new Error(`expected 1 grayscale channel, got ${info.channels}`);
  }

  const result = Buffer.alloc(size * size, 255);
  const xOffset = Math.floor((size - newWidth) / 2);
  const yOffset = Math.floor((size - newHeight) / 2);

  for (let y = 0; y < newHeight; y++) {
    for (let x = 0; x < newWidth; x++) {
      result[(y + yOffset) * size + (x + xOffset)] = resized[y * newWidth + x];
    }
  }

  const bytesData = [];
  for (let row = 0; row < size; row++) {
    for (let byteCol = 0; byteCol < size / 8; byteCol++) {
      let byteVal = 0;
      for (let bit = 0; bit < 8; bit++) {
        let sourceX = byteCol * 8 + bit;
        let sourceY = row;
        if (rotate === 90) {
          sourceX = row;
          sourceY = size - 1 - (byteCol * 8 + bit);
        } else if (rotate === 180) {
          sourceX = size - 1 - (byteCol * 8 + bit);
          sourceY = size - 1 - row;
        } else if (rotate === 270) {
          sourceX = size - 1 - row;
          sourceY = byteCol * 8 + bit;
        }
        const gray = result[sourceY * size + sourceX];
        const isWhite = invert ? gray < threshold : gray >= threshold;
        if (isWhite) {
          byteVal |= 1 << (7 - bit);
        }
      }
      bytesData.push(byteVal);
    }
  }

  let output = "#pragma once\n";
  output += "#include <cstdint>\n";
  output += "\n";
  output += `// node scripts/convert-logo.mjs <input> <output> --size ${size} --name ${name} --rotate ${rotate}\n`;
  output += `inline constexpr int ${name}Size = ${size};\n`;
  output += `inline constexpr uint8_t ${name}[] = {\n`;

  for (let i = 0; i < bytesData.length; i++) {
    if (i % 19 === 0) {
      output += "    ";
    }
    output += `0x${bytesData[i].toString(16).toUpperCase().padStart(2, "0")}`;
    const last = i === bytesData.length - 1;
    const endRow = (i + 1) % 19 === 0;
    if (!last) {
      output += ",";
    }
    output += endRow || last ? "\n" : " ";
  }


  output += "};\n";

  const dir = path.dirname(outputPath);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }

  fs.writeFileSync(outputPath, output);

  return { size, name, bytes: bytesData.length };
}

async function main() {
  const { values, positionals } = parseArgs({
    allowPositionals: true,
    options: {
      invert: { type: "boolean", default: false },
      threshold: { type: "string", default: "128" },
      rotate: { type: "string", default: "270" },
      size: { type: "string", default: String(DEFAULT_SIZE) },
      name: { type: "string", default: DEFAULT_NAME },
      help: { type: "boolean", short: "h", default: false },
    },
  });

  if (values.help || positionals.length === 0) {
    console.log(`
Convert image to C header logo format (1-bit, white background)

Usage:
  node convert-logo.mjs <input> [output] [options]

Arguments:
  input     Input image (PNG, JPG, etc.)
  output    Output header file (default: src/images/PapyrixLogo.h)

Options:
  --size <n>         Square size in pixels, multiple of 8 (default: 384)
  --name <ident>     C identifier prefix (default: PapyrixLogo)
  --invert           Invert colors (black becomes white)
  --threshold <n>    Threshold for black/white (0-255, default: 128)
  --rotate <deg>     Rotate clockwise (0, 90, 180, 270; default: 270)
  -h, --help         Show this help message

Examples:
  node convert-logo.mjs logo.png
  node convert-logo.mjs logo.png src/images/MyLogo.h
  node convert-logo.mjs logo.png --invert --threshold 100
  node convert-logo.mjs ../images/localsend-logo.png ../src/images/LocalsendLogo.h --size 192 --name LocalsendLogo --rotate 0
`);
    process.exit(0);
  }

  const inputPath = positionals[0];
  const outputPath = positionals[1] || "../src/images/PapyrixLogo.h";
  const threshold = parseInt(values.threshold, 10);
  const rotate = parseInt(values.rotate, 10);
  const size = parseInt(values.size, 10);

  if (!fs.existsSync(inputPath)) {
    console.error(`Error: Input file not found: ${inputPath}`);
    process.exit(1);
  }

  try {
    const result = await convertToLogo(inputPath, outputPath, {
      invert: values.invert,
      threshold,
      rotate,
      size,
      name: values.name,
    });
    console.log(`Created: ${outputPath}`);
    console.log(`  Size: ${result.size}x${result.size}`);
    console.log(`  Bytes: ${result.bytes}`);
  } catch (error) {
    console.error(`Error: ${error.message}`);
    process.exit(1);
  }
}

main();
