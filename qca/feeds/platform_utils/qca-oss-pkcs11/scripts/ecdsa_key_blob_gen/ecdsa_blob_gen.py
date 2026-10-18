# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC

import os
import shutil
import struct
import subprocess
import sys
import argparse
import textwrap


# Define constants
MAX_ECC_PUB_KEY_LEN = 133
PAD_LEN = 3
MAX_ECC_ENC_PVT_KEY_LEN = 80
MAX_IV_SIZE = 16
MAX_CMAC_AES_SIZE = 16
MAX_CONTEXT_LEN = 128

# ---ECDSA Blob Structre---
#
# struct ECDSA_import_blob{
#  uint32_t curve_mode;
#  uint8_t public_key[MAX_ECC_PUB_KEY_LEN];
#  uint8_t padding[PAD_LEN];
#  uint32_t public_key_len;
#  uint8_t encrypted_private_key[MAX_ECC_ENC_PVT_KEY_LEN];
#  uint32_t encrypted_private_key_len;
#  uint8_t context[MAX_CONTEXT_LEN];
#  uint32_t context_len;
#  uint8_t iv_data[MAX_IV_SIZE];
#  uint32_t iv_data_len;
#  uint8_t cmac[MAX_CMAC_AES_SIZE];
# }ECDSA_import_blob_t;

# MAX_ECC_PUB_KEY_LEN 136 # 133
# PAD_LEN = 3
# MAX_ECC_ENC_PVT_KEY_LEN 80
# MAX_CONTEXT_LEN 128
# MAX_IV_SIZE 16
# MAX_CMAC_AES_SIZE 16

# Define the struct format with CMAC
struct_format_with_cmac = f'<I{MAX_ECC_PUB_KEY_LEN}s{PAD_LEN}sI{MAX_ECC_ENC_PVT_KEY_LEN}sI{MAX_CONTEXT_LEN}sI{MAX_IV_SIZE}sI{MAX_CMAC_AES_SIZE}s'

# Define the struct format without CMAC
struct_format_without_cmac = f'<I{MAX_ECC_PUB_KEY_LEN}s{PAD_LEN}sI{MAX_ECC_ENC_PVT_KEY_LEN}sI{MAX_CONTEXT_LEN}sI{MAX_IV_SIZE}sI'

# Function to read binary file
def read_binary_file(file_path, size):
    with open(file_path, 'rb') as f:
        data = f.read(size)
    return data

# Function to write the struct data into a new binary file
def write_binary_file(file_path, struct_data, struct_format):
    packed_data = struct.pack(struct_format, *struct_data)
    with open(file_path, 'wb') as f:
        f.write(packed_data)

def main():
    # Create the parser
    parser = argparse.ArgumentParser(
    description="Generate ECDSA Blob with CMAC",
    epilog="Example Usage: python3 ecdsa_blob_gen.py -k 5e3ccb930a48481a7e5dc05aa74c73043a3139eb9d513ae54374b436accf3320 -i 3fa2ed59865434813a07ef4dc27376d0 -c c118b12720b87069b55756cea6d07ea164d2739bc853e220073a02a30c8f5a1e376f272928dfe90bcc2299e1c8544f6983c4ae3b8ed98c4fe0a2b78a009df8d29da50a67d9eefca88173987483132ae8d12895f717d8f66cd7a566bdfe98ee182f26e556cdeac6102f7c4fc1cc7e95c548ef623e1cacdb4a24b962b388dda2dc -m secp384r1")

    # Add arguments
    parser.add_argument('-k', '--key', type=str, required=True, help='32-byte AES Key generated using NIST tool. Used for EC Private key encryption and CMAC')
    parser.add_argument('-e', '--eckey', type=str, required=False, help='Optional path to an existing ECDSA private key PEM file')
    parser.add_argument('-i', '--iv', type=str, required=True, help='16-byte IV data used for EC Private key encryption')
    parser.add_argument('-c', '--context', type=str, required=True, help='128-byte context used during AES Key generation using NIST Tool')
    parser.add_argument('-m', '--curve_mode', type=str, choices=['secp256r1', 'secp384r1', 'secp521r1'], required=True, help='Curve mode (secp256r1, secp384r1, secp521r1)')

    # Parse the arguments
    args = parser.parse_args()

    # Input validation
    if len(args.key) != 64:
        print("Error: hex_key must be 32 bytes (64 hex characters).")
        print(f"Given length: {len(args.key)}")
        sys.exit(1)

    if len(args.iv) != 32:
        print("Error: hex_iv must be 16 bytes (32 hex characters).")
        print(f"Given length: {len(args.iv)}")
        sys.exit(1)

    if len(args.context) != 256:
        print("Error: hex_context must be 128 bytes (256 hex characters).")
        print(f"Given length: {len(args.context)}")
        sys.exit(1)

    print("Starting script...")

    # Create temp directory if it doesn't exist
    temp_dir = 'temp'
    os.makedirs(temp_dir, exist_ok=True)

    # Step 1: Use provided ECDSA private key or generate a new one
    ec_private_key_pem = os.path.join(temp_dir, "ec_private_key.pem")

    if args.eckey:
        print(f"Using provided ECDSA private key: {args.eckey}")
        shutil.copy(args.eckey, ec_private_key_pem)
    else:
        print("Generating ECDSA private key...")
        subprocess.run(["openssl", "ecparam", "-name", args.curve_mode, "-genkey", "-noout", "-out",ec_private_key_pem ])


    # Step 2: Convert to DER format
    print("Converting to DER format...")
    subprocess.run(["openssl", "ec", "-in", os.path.join(temp_dir, "ec_private_key.pem"), "-outform", "DER", "-out", os.path.join(temp_dir, "ec_private_key.der")])

    # Step 3: Get private key as binary
    print("Get ECDSA private key as binary...")
    openssl_command = [
        'openssl', 'ec', '-in', os.path.join(temp_dir, 'ec_private_key.der'), '-inform', 'DER', '-noout', '-text'
    ]
    awk_command = "awk '/priv:/{flag=1;next}/pub:/{flag=0}flag'"
    xxd_command = ['xxd', '-r', '-p']
    command = f"{' '.join(openssl_command)} | {awk_command} | {' '.join(xxd_command)} > {os.path.join(temp_dir, 'raw_private_key.bin')}"
    subprocess.run(command, shell=True)

    # Step 4: Get public key as binary
    print("Get ECDSA public key as binary...")
    openssl_command = [
        'openssl', 'ec', '-in', os.path.join(temp_dir, 'ec_private_key.der'), '-inform', 'DER', '-noout', '-text'
    ]
    awk_command = "awk '/pub:/{flag=1;next}/ASN1 OID:/{flag=0}flag'"
    xxd_command = ['xxd', '-r', '-p']
    command = f"{' '.join(openssl_command)} | {awk_command} | {' '.join(xxd_command)} > {os.path.join(temp_dir, 'raw_public_key.bin')}"
    subprocess.run(command, shell=True)

    # Step 5: Encrypt the private key bin with AES
    print("Encrypting the private key with AES...")
    subprocess.run(["openssl", "enc", "-aes-256-cbc", "-in", os.path.join(temp_dir, "raw_private_key.bin"), "-out", os.path.join(temp_dir, "encrypted_private_key.bin"), "-K", args.key, "-iv", args.iv])

    print("Reading binary files...")

    private_key_file_path = os.path.join(temp_dir, 'encrypted_private_key.bin')
    public_key_file_path = os.path.join(temp_dir, 'raw_public_key.bin')

    # Read the binary files
    private_key = read_binary_file(private_key_file_path, MAX_ECC_ENC_PVT_KEY_LEN)
    public_key = read_binary_file(public_key_file_path, MAX_ECC_PUB_KEY_LEN)
    context = bytes.fromhex(args.context)
    iv_data = bytes.fromhex(args.iv)
    # Set curve_mode based on args.curve_mode
    if args.curve_mode == 'secp256r1':
        curve_mode = 0
    elif args.curve_mode == 'secp384r1':
        curve_mode = 1
    elif args.curve_mode == 'secp521r1':
        curve_mode = 2
    private_key_len = len(private_key)
    public_key_len = len(public_key)
    context_len = len(context)
    iv_data_len = len(iv_data)
    pad = b'\x00' * PAD_LEN

    # Define the struct data without CMAC
    ecdsa_blob_struct_without_cmac = (curve_mode, public_key, pad, public_key_len, private_key, private_key_len, context, context_len, iv_data, iv_data_len)

    print("Writing struct data to binary file without CMAC...")
    output_file_path_without_cmac = os.path.join(temp_dir, 'ecdsa_blob_without_cmac.bin')
    write_binary_file(output_file_path_without_cmac, ecdsa_blob_struct_without_cmac, struct_format_without_cmac)
    print(f"The struct data has been written to {output_file_path_without_cmac}.")

    # CMAC calculation
    command = [
        "openssl", "dgst", "-mac", "cmac",
        "-macopt", "cipher:aes-256-cbc",
        f"-macopt", f"hexkey:{args.key}",
        os.path.join(temp_dir, "ecdsa_blob_without_cmac.bin")
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    output_lines = result.stdout.splitlines()
    cmac_hash = output_lines[-1].split()[-1] if output_lines else None
    cmac = bytes.fromhex(cmac_hash)
    cmac_len = len(cmac)

    # Define the struct data with CMAC
    ecdsa_blob_struct_with_cmac = (curve_mode, public_key, pad, public_key_len, private_key, private_key_len, context, context_len, iv_data, iv_data_len, cmac)

    print("Writing struct data to binary file with CMAC...")
    output_file_path_with_cmac = os.path.join(temp_dir, 'ecdsa_blob_with_cmac.bin')
    write_binary_file(output_file_path_with_cmac, ecdsa_blob_struct_with_cmac, struct_format_with_cmac)
    print(f"The struct data has been written to {output_file_path_with_cmac}.")

    # Create output directory if it doesn't exist
    output_dir = 'output'
    os.makedirs(output_dir, exist_ok=True)

    # Copy the required files to the output directory
    shutil.copy(os.path.join(temp_dir, 'ecdsa_blob_with_cmac.bin'), output_dir)
    shutil.copy(os.path.join(temp_dir, 'ec_private_key.pem'), output_dir)

    print(f"\nCopied ec_private_key.pem and ecdsa_blob_with_cmac.bin to {output_dir} directory.")
    print(f"\nOutput:\n ec_private_key.pem --> EC key, can be used for offline verification\n ecdsa_blob_with_cmac.bin --> ECDSA Blob understandable by CRYPTO TA for importing EC Key\n")

if __name__ == "__main__":
    main()
