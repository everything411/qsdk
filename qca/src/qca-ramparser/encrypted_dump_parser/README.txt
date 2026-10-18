: '
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: GPL-2.0-only
'

================================================================================
ENCRYPTED DUMP PARSER TOOLS
================================================================================

This directory contains tools for parsing, encrypting, and decrypting encrypted
crash dumps and filesystem images.

================================================================================
TOOLS OVERVIEW
================================================================================

1. crypto_context_parser.py
   - Parses CRYPTO_CONTEXT.BIN files
   - Extracts encryption mode and cryptographic contexts
   - Supports both ECB and XTS modes

2. crypt_rootfs_xts
   - Encrypts/decrypts files using AES-XTS-plain64 (dm-crypt standard)
   - Uses 512-byte sectors with continuous sector numbering
   - Compatible with standard dm-crypt encrypted filesystems

3. decrypt_ecb_mode.py
   - Decrypts files encrypted with AES-ECB mode
   - Automatically detects and removes padding from gzip files
   - Uses OpenSSL for decryption

================================================================================
DETAILED USAGE
================================================================================

--------------------------------------------------------------------------------
1. CRYPTO_CONTEXT_PARSER.PY
--------------------------------------------------------------------------------

Purpose:
  Parse CRYPTO_CONTEXT.BIN files to extract encryption parameters.

Usage:
  python3 crypto_context_parser.py [path_to_crypto_context.bin]

Example:
  python3 crypto_context_parser.py DUMP_ENC/CRYPTO_CONTEXT.BIN

Output:
  - Displays encryption mode (e.g., aes-xts-256, aes-ecb-128)
  - Shows Context1 and Context2 (for XTS mode) in hex format
  - Indicates whether it's XTS or ECB mode

File Format:
  CRYPTO_CONTEXT.BIN contains:
  - Mode string (ASCII) followed by space (0x20)
  - Context1 (128 bytes binary data)
  - For XTS mode: space (0x20) + Context2 (128 bytes)

--------------------------------------------------------------------------------
2. CRYPT_ROOTFS_XTS
--------------------------------------------------------------------------------

Purpose:
  Encrypt or decrypt files using AES-XTS-plain64 algorithm (dm-crypt standard).

Usage:
  python3 crypt_rootfs_xts.py decrypt <input_file> <key_file> <output_file>

Examples:
  # Decrypt a file
  python3 crypt_rootfs_xts.py decrypt encrypted.img key.bin decrypted.img

Features:
  - AES-XTS-plain64 encryption/decryption
  - 512-byte sector size
  - Continuous sector numbering (0, 1, 2, 3...)
  - Supports AES-128 (32-byte key) and AES-256 (64-byte key)
  - Generates dm-crypt table entry (.dm file) when encrypting
  - Minimal padding (up to 4KB)

Key File:
  - Binary file containing the encryption key
  - 32 bytes for AES-128
  - 64 bytes for AES-256

Output (Encryption):
  - Encrypted file
  - .dm file containing crypttab entry for dm-crypt

--------------------------------------------------------------------------------
3. DECRYPT_ECB_MODE.PY
--------------------------------------------------------------------------------

Purpose:
  Decrypt files encrypted with AES-ECB mode and clean up gzip padding.

Usage:
  python3 decrypt_ecb_mode.py -i <input.gz> -k <key> -m <mode> [-o <output.gz>]

Examples:
  # AES-128-ECB mode
  python3 decrypt_ecb_mode.py \
    -i DUMP_ENC/EBICS0.BIN.gz \
    -k b1550a45e23575c068de2de9705c9009 \
    -m aes-128-ecb

  # AES-256-ECB mode with custom output
  python3 decrypt_ecb_mode.py \
    -i DUMP_ENC/EBICS0.BIN.gz \
    -k b1550a45e23575c068de2de9705c9009b1550a45e23575c068de2de9705c9099 \
    -m aes-256-ecb \
    -o EBICS0_clean.gz

Features:
  - Decrypts using OpenSSL
  - Automatically detects gzip files
  - Removes padding after gzip footer
  - Validates CRC32 checksum
  - Handles non-gzip files gracefully

Supported Modes:
  - aes-128-ecb (16-byte key in hex)
  - aes-256-ecb (32-byte key in hex)

Output:
  - Clean decrypted file (padding removed for gzip files)
  - Detailed analysis of gzip structure

================================================================================
DEPENDENCIES
================================================================================

Python Packages:
  - Python Version : Python3 required
  - cryptography (for AES encryption/decryption)
    Install: pip3 install cryptography
    Or: apt-get install python3-cryptography

System Tools:
  - OpenSSL (for decrypt_ecb_mode.py)
    Usually pre-installed on Linux systems

================================================================================
KEY FILE FORMATS
================================================================================

Binary Key File(for XTS mode):
  - Raw binary data
  - 32 bytes for AES-128
  - 64 bytes for AES-256
  - Example: key.bin

Hex Key (for ECB mode):
  - Hexadecimal string
  - 32 hex characters for AES-128 (16 bytes)
  - 64 hex characters for AES-256 (32 bytes)
  - Example: b1550a45e23575c068de2de9705c9009

================================================================================
