#!/usr/bin/env python3
: '
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: GPL-2.0-only
'
"""
Decrypt and Clean Gzip Tool

Decrypts an encrypted gzip file using OpenSSL, detects padding after
the gzip footer,
and produces a clean gzip file.

Usage:
    python3 decrypt_and_clean_gzip.py -i <input.gz> -k <key> -m <mode>
                                      [-o <output.gz>]

Example:
    python3 decrypt_and_clean_gzip.py -i EBICS0.BIN.gz
            -k b1550a45e23575c068de2de9705c9009 -m aes-128-ecb
"""

import sys
import struct
import zlib
import subprocess
import argparse
from pathlib import Path


def decrypt_file(input_file, key, mode, temp_output):
    """Decrypt file using OpenSSL with specified mode."""
    cmd = [
        'openssl', 'enc', f'-{mode}', '-d',
        '-in', str(input_file),
        '-out', str(temp_output),
        '-K', key,
        '-nopad'
    ]

    print(f"Decrypting: {input_file}")
    print(f"Command: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(f"✓ Decryption successful: {temp_output}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"✗ Decryption failed: {e.stderr}")
        return False


def is_gzip_file(filepath):
    """Check if file is a valid gzip file by checking magic bytes."""
    GZIP_MAGIC = b'\x1f\x8b'
    try:
        with open(filepath, 'rb') as f:
            magic = f.read(2)
            return magic == GZIP_MAGIC
    except Exception:
        return False


def analyze_gzip(filepath):
    """Find gzip footer and detect padding."""
    with open(filepath, 'rb') as f:
        data = f.read()

    # Decompress to find actual gzip end
    decompressor = zlib.decompressobj(16 + zlib.MAX_WBITS)
    uncompressed = decompressor.decompress(data)
    padding = decompressor.unused_data

    # Calculate positions
    file_size = len(data)
    gzip_end = file_size - len(padding)
    footer_start = gzip_end - 8

    # Parse footer
    crc32, isize = struct.unpack('<II', data[footer_start:gzip_end])
    calc_crc32 = zlib.crc32(uncompressed) & 0xffffffff

    return {
        'file_size': file_size,
        'gzip_end': gzip_end,
        'footer_start': footer_start,
        'padding_size': len(padding),
        'crc32_valid': crc32 == calc_crc32,
        'uncompressed_size': len(uncompressed),
        'padding_bytes': padding[:32] if padding else b''
    }


def process_decrypted_file(input_path, output_path):
    """Process decrypted file - check if gzip and remove padding if
    applicable."""

    # Check if it's a gzip file
    if not is_gzip_file(input_path):
        print(f"\n⚠️  Decrypted file is NOT a gzip file")
        print(f"  File: {input_path}")

        # Read first 16 bytes to show file type
        with open(input_path, 'rb') as f:
            header = f.read(16)
        print(f"  Header (hex): {header.hex()}")

        # Just copy the decrypted file as-is
        import shutil
        shutil.copy2(input_path, output_path)

        file_size = input_path.stat().st_size
        print(f"\n✓ Decrypted file saved as-is: {output_path} "
              f"({file_size} bytes)")
        print("  (No gzip processing performed)")
        return True

    # It's a gzip file, proceed with padding analysis and removal
    print(f"\n✓ Decrypted file is a valid gzip file")

    try:
        info = analyze_gzip(input_path)

        print(f"\nAnalyzing gzip file: {input_path}")
        print(f"  File size: {info['file_size']} bytes")
        print(f"  Gzip ends at: {info['gzip_end']} bytes")
        print(f"  Footer: bytes {info['footer_start']}-{info['gzip_end']-1}")
        print(f"  CRC32: {'✓ Valid' if info['crc32_valid'] else '✗ Invalid'}")
        print(f"  Uncompressed: {info['uncompressed_size']} bytes")

        if info['padding_size'] > 0:
            print(f"  Padding: {info['padding_size']} bytes detected")
            if info['padding_bytes']:
                print(f"  Padding preview: {info['padding_bytes'].hex()}")
        else:
            print(f"  ✓ No padding detected")

        if not info['crc32_valid']:
            print("\n✗ Error: CRC32 mismatch - file may be corrupted")
            return False

        # Write clean gzip
        with open(input_path, 'rb') as fin:
            with open(output_path, 'wb') as fout:
                fout.write(fin.read(info['gzip_end']))

        if info['padding_size'] > 0:
            print(f"\n✓ Removed {info['padding_size']} bytes of padding")

        print(f"✓ Clean gzip saved: {output_path} ({info['gzip_end']} bytes)")
        return True

    except zlib.error as e:
        print(f"\n✗ Error decompressing gzip: {e}")
        print("  File may be corrupted or not a valid gzip")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Decrypt encrypted gzip and remove padding',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # AES-128-ECB mode
  python3 decrypt_and_clean_gzip.py \\
    -i DUMP_ENC/EBICS0.BIN.gz \\
    -k b1550a45e23575c068de2de9705c9009 \\
    -m aes-128-ecb

  # AES-256-ECB mode with custom output
  python3 decrypt_and_clean_gzip.py \\
    -i DUMP_ENC/EBICS0.BIN.gz \\
    -k b1550a45e23575c068de2de9705c9009 b1550a45e23575c068de2de9705c9099\\
    -m aes-256-ecb \\
    -o EBICS0_clean.gz
        """
    )

    parser.add_argument('-i', '--input', required=True, type=Path,
                       help='Input encrypted .gz file')
    parser.add_argument('-k', '--key', required=True,
                       help='Decryption key (hex)')
    parser.add_argument('-m', '--mode', required=True,
                       choices=['aes-128-ecb', 'aes-256-ecb'],
                       help='Encryption mode (aes-128-ecb or aes-256-ecb)')
    parser.add_argument('-o', '--output', type=Path,
                       help='Output clean .gz file (default: <input>_clean.gz)')

    args = parser.parse_args()

    # Validate input file
    if not args.input.exists():
        print(f"✗ Error: Input file not found: {args.input}")
        sys.exit(1)

    # Determine output file
    if args.output:
        final_output = args.output
    else:
        final_output = args.input.with_name(
            f"{args.input.stem}_clean{args.input.suffix}")

    # Create temporary decrypted file
    temp_decrypted = args.input.with_name(
        f"{args.input.stem}_decrypted{args.input.suffix}")

    try:
        # Step 1: Decrypt
        print("=" * 70)
        print("STEP 1: DECRYPTION")
        print("=" * 70)
        print(f"Mode: {args.mode}")
        if not decrypt_file(args.input, args.key, args.mode, temp_decrypted):
            sys.exit(1)

        # Step 2: Check file type and process accordingly
        print("\n" + "=" * 70)
        print("STEP 2: FILE TYPE CHECK AND PROCESSING")
        print("=" * 70)
        if not process_decrypted_file(temp_decrypted, final_output):
            sys.exit(1)

        # Summary
        print("\n" + "=" * 70)
        print("SUMMARY")
        print("=" * 70)
        print(f"Input (encrypted):  {args.input}")
        print(f"Temp (decrypted):   {temp_decrypted}")
        print(f"Output (clean):     {final_output}")
        print("\n✓ Process completed successfully!")
        print("=" * 70)

        # Cleanup temp file
        print(f"\nCleaning up temporary file: {temp_decrypted}")
        temp_decrypted.unlink()

    except Exception as e:
        print(f"\n✗ Error: {e}")
        # Cleanup on error
        if temp_decrypted.exists():
            temp_decrypted.unlink()
        sys.exit(1)

if __name__ == '__main__':
    main()
