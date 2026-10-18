#!/usr/bin/env python3
: '
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: GPL-2.0-only
'

"""
Root filesystem decryption tool.

Usage: crypt_rootfs_xts.py decrypt <input_file> <key_file>
                           <output_file>

Implements aes-xts-plain64 decryption algorithm of dm-crypt,
runs fully in userspace.

Examples:
    # Decrypt a file
    python3 crypt_rootfs_xts.py decrypt encrypted.img key.bin decrypted.img

"""

import os
import sys
from typing import BinaryIO

# Install "python3-cryptography" on Debian/Ubuntu or "cryptography" from PyPi
from cryptography.hazmat.primitives.ciphers import Cipher
from cryptography.hazmat.primitives.ciphers.algorithms import AES
from cryptography.hazmat.primitives.ciphers.modes import XTS
from cryptography.hazmat.backends import default_backend

SECTOR_SIZE = 512

_backend = default_backend()


def _encrypt_with_tweak(plaintext: bytes, aes: AES, tweak: int) -> bytes:
    """
    Encrypt a single disk sector with the specified sector index (tweak).

    For more details on the "aes-xts-plain64" and other crypt cipher
    algorithms, see
    https://gitlab.com/cryptsetup/cryptsetup/-/wikis/DMCrypt

    :param plaintext: Unencrypted sector data
    :param aes: Instance of the AES cipher with a preconfigured
                encryption key
    :param tweak: Sector index/XTS tweak
    :return: Encrypted sector data
    """

    # "aes-xts-plain64" cipher summary:
    #   aes - AES block cipher
    #   xts - XTS block cipher chaining mode
    #   plain64 - the initial vector is the 64-bit little-endian
    #             version of the sector number, padded with zeros if
    #     necessary.
    #
    # Key size for XTS mode is double of that for the underlying
    # block cipher. Broadcom uses a 32-byte key, which
    # corresponds to AES-128 level of security.

    tweak_bytes = tweak.to_bytes(length=16, byteorder='little')
    encryptor = Cipher(algorithm=aes, mode=XTS(tweak_bytes),
                       backend=_backend).encryptor()
    output = encryptor.update(plaintext) + encryptor.finalize()
    return output


def _decrypt_with_tweak(ciphertext: bytes, aes: AES, tweak: int) -> bytes:
    """
    Decrypt a single disk sector with the specified sector index (tweak).

    For more details on the "aes-xts-plain64" and other crypt cipher algorithms, see
    https://gitlab.com/cryptsetup/cryptsetup/-/wikis/DMCrypt#mapping-table-for-crypt-target

    :param ciphertext: Encrypted sector data
    :param aes: Instance of the AES cipher with a preconfigured
                decryption key
    :param tweak: Sector index/XTS tweak
    :return: Decrypted sector data
    """

    tweak_bytes = tweak.to_bytes(length=16, byteorder='little')
    decryptor = Cipher(algorithm=aes, mode=XTS(tweak_bytes),
                       backend=_backend).decryptor()
    output = decryptor.update(ciphertext) + decryptor.finalize()
    return output


def encrypt(plain_file: BinaryIO, encrypted_file: BinaryIO, key: bytes):
    """
    Encrypt a disk image with the "aes-xts-plain64" mode of dm-crypt
    with the specified key.

    :param plain_file: Source unencrypted binary file object of the
                       firmware image
    :param encrypted_file: Target encrypted binary file object of the
                           firmware image
    :param key: Encryption key (32 bytes for AES-128 or 64 bytes for AES-256)
    """

    algorithm = AES(key)
    sector_counter = 0

    while True:
        sector_plain = plain_file.read(SECTOR_SIZE)
        if not sector_plain:
            break

        # Pad data to SECTOR_SIZE - needed for encryption
        if len(sector_plain) < SECTOR_SIZE:
            sector_plain += (b'\x00' * (SECTOR_SIZE - len(sector_plain)))

        encrypted_file.write(_encrypt_with_tweak(sector_plain, algorithm,
                                                  sector_counter))
        sector_counter += 1

    plain_size = sector_counter * SECTOR_SIZE

    # Encrypted image padding as defined by Broadcom
    if (plain_size % (1024 * 4) != 0):
        pad_length = ((plain_size // 1024) + 4) * 1024 - plain_size
        encrypted_file.write(b'\x00' * pad_length)


def decrypt(encrypted_file: BinaryIO, decrypted_file: BinaryIO, key: bytes):
    """
    Decrypt a disk image with the "aes-xts-plain64" mode of dm-crypt
    with the specified key.

    :param encrypted_file: Source encrypted binary file object of the
                           firmware image
    :param decrypted_file: Target decrypted binary file object of the
                           firmware image
    :param key: Decryption key (32 bytes for AES-128 or 64 bytes for AES-256)
    """

    algorithm = AES(key)
    sector_counter = 0

    while True:
        sector_encrypted = encrypted_file.read(SECTOR_SIZE)
        if not sector_encrypted:
            break

        # Pad data to SECTOR_SIZE if needed
        # (should not be necessary for properly encrypted files)
        if len(sector_encrypted) < SECTOR_SIZE:
            sector_encrypted += (b'\x00' * (SECTOR_SIZE - len(sector_encrypted)))

        decrypted_file.write(_decrypt_with_tweak(sector_encrypted,
                                                  algorithm,
                                                  sector_counter))
        sector_counter += 1


def generate_dm_entry(encrypted_file_name: str, key: bytes) -> str:
    """
    Generate a crypt table (crypttab) mapping line for the specified
    encrypted image.

    For more details on the crypt table mapping line syntax, see:
    https://gitlab.com/cryptsetup/cryptsetup/-/wikis/DMCrypt

    :param encrypted_file_name: File name of the encrypted disk image
    :param key: Encryption key
    :return: Crypt table mapping line for crypttab
    """

    encrypted_size = os.stat(encrypted_file_name).st_size
    sector_count, remainder = divmod(encrypted_size, SECTOR_SIZE)
    assert remainder == 0, ('Encrypted file length must be a multiple '
                            'of the predefined sector size')

    # The syntax of the crypttab entry is as follows:
    # <start_sector> <size> <target name> <target mapping table>
    # <target mapping table>: <cipher> <key> <iv_offset> <device path>
    #                         <offset> [<#opt_params> <opt_params>]
    # In our particular case:
    #   start_sector = 0
    #   size = <sector_count>
    #   target name = crypt (for dm-crypt)
    #   cipher = aes-xts-plain64
    #   key = <encryption key in hex>
    #   iv_offset = 0
    #   device path = 7:11
    #   offset = 0
    # Device path 7:11 corresponds to a loop block device (major 7)
    # of index 11 (minor), corresponding to /dev/loop11.
    # This value can be different, since it varies depending on where
    # you're building the encrypted image, and
    # will be ignored by subsequent steps of the Broadcom image builder.
    return f'0 {sector_count} crypt aes-xts-plain64 {key.hex()} 0 7:11 0'


if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("Usage: crypt_rootfs_xts <encrypt|decrypt> <input_file> "
              "<key_file> <output_file>")
        print("\nExamples:")
        print("  # Encrypt a file")
        print("  python3 crypt_rootfs_xts encrypt plaintext.img "
              "key.bin encrypted.img")
        print("")
        print("  # Decrypt a file")
        print("  python3 crypt_rootfs_xts decrypt encrypted.img "
              "key.bin decrypted.img")
        print("\nDescription:")
        print("  Encrypts or decrypts an image file using "
              "AES-XTS-plain64 algorithm")
        print("  - operation: 'encrypt' or 'decrypt'")
        print("  - input_file: Path to the input file")
        print("  - key_file: Path to the encryption key file "
              "(32 or 64 bytes)")
        print("  - output_file: Path where output file will be saved")
        sys.exit(1)

    operation = sys.argv[1].lower()
    input_file = sys.argv[2]
    key_file = sys.argv[3]
    output_file = sys.argv[4]

    # Validate operation
    if operation not in ['encrypt', 'decrypt']:
        print(f"Error: Invalid operation '{operation}'. "
              f"Must be 'encrypt' or 'decrypt'")
        sys.exit(1)

    # Verify input files exist
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)

    if not os.path.exists(key_file):
        print(f"Error: Key file '{key_file}' not found")
        sys.exit(1)

    # Read the key
    try:
        with open(key_file, 'rb') as f:
            key_data = f.read()

        if len(key_data) not in [32, 64]:
            print(f"Warning: Key size is {len(key_data)} bytes. "
                  f"Expected 32 (AES-128) or 64 (AES-256) bytes.")
    except Exception as e:
        print(f"Error reading key file: {e}")
        sys.exit(1)

    # Perform encryption or decryption
    try:
        with open(input_file, 'rb') as input_f, \
             open(output_file, 'wb') as output_f:
            if operation == 'encrypt':
                encrypt(input_f, output_f, key_data)
            else:  # decrypt
                decrypt(input_f, output_f, key_data)

        # Set file ownership if running as root
        uid = os.getuid()
        if not uid:
            uid = int(os.getenv('SUDO_UID', 0))
        if uid:
            os.chown(output_file, uid=uid, gid=-1)

        print(f"✓ {operation.capitalize()}ion completed successfully!")
        print(f"  Input:  {input_file}")
        print(f"  Output: {output_file}")
        print(f"  Key:    {key_file} ({len(key_data)} bytes)")

        # Generate dm entry for encryption
        if operation == 'encrypt':
            dm_entry = generate_dm_entry(output_file, key_data)
            dm_file = output_file + ".dm"
            with open(dm_file, 'w') as f:
                f.write(dm_entry + '\n')
            if uid:
                os.chown(dm_file, uid=uid, gid=-1)
            print(f"  DM entry saved to: {dm_file}")

    except Exception as e:
        print(f"Error during {operation}ion: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
