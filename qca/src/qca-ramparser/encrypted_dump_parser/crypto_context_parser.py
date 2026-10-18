#!/usr/bin/env python3

: '
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: GPL-2.0-only
'

"""
CRYPTO_CONTEXT.BIN Parser

This script parses the CRYPTO_CONTEXT.BIN file which contains:
- Mode (string): encryption mode (e.g., "aes-xts-256", "aes-ecb-256")
- Context1 (128 bytes hex): first cryptographic context
- Context2 (128 bytes hex): second cryptographic context (only for XTS mode)

File format:
- Mode string (ASCII) followed by space (0x20)
- Context1 (128 bytes binary data) followed by space (0x20) [for XTS mode]
- Context2 (128 bytes binary data) until EOF [for XTS mode only]

For ECB mode: Mode + space + Context1 (128 bytes)
For XTS mode: Mode + space + Context1 (128 bytes) + space + Context2 (128 bytes)

Usage: python3 crypto_context_parser.py [path_to_crypto_context.bin]
"""

import sys
import os
from typing import Tuple, Optional

CONTEXT_SIZE = 128  # Each context is exactly 128 bytes

class CryptoContextParser:
    def __init__(self, file_path: str):
        """
        Initialize the parser with the path to CRYPTO_CONTEXT.BIN file.

        Args:
            file_path: Path to the CRYPTO_CONTEXT.BIN file
        """
        self.file_path = file_path
        self.mode = None
        self.context1 = None
        self.context2 = None
        self.raw_data = None

    def parse(self) -> bool:
        """
        Parse the CRYPTO_CONTEXT.BIN file.

        Returns:
            True if parsing was successful, False otherwise
        """
        try:
            with open(self.file_path, 'rb') as f:
                self.raw_data = f.read()

            if not self.raw_data:
                print(f"Error: File {self.file_path} is empty")
                return False

            # Find the first space separator (0x20)
            # This separates mode from context data
            first_space_pos = self.raw_data.find(b' ')

            if first_space_pos == -1:
                print("Error: No space separator found after mode string")
                return False

            # Extract mode (first part before first space)
            try:
                self.mode = self.raw_data[:first_space_pos].decode('ascii')
            except UnicodeDecodeError:
                print("Error: Mode is not valid ASCII")
                return False

            # Calculate positions for contexts
            # Context1 starts right after the first space
            context1_start = first_space_pos + 1
            context1_end = context1_start + CONTEXT_SIZE

            # Check if we have enough data for Context1
            if context1_end > len(self.raw_data):
                print(f"Error: File too short. Need at least "
                      f"{context1_end} bytes for mode + Context1")
                return False

            # Extract Context1 (exactly 128 bytes)
            self.context1 = self.raw_data[context1_start:context1_end]

            # Check if there's a second context (XTS mode)
            # There should be a space separator after Context1
            if context1_end < len(self.raw_data):
                # Check if the next byte is a space (separator for XTS mode)
                if self.raw_data[context1_end] == 0x20:
                    # XTS mode: has two contexts
                    context2_start = context1_end + 1
                    context2_end = context2_start + CONTEXT_SIZE

                    # Check if we have enough data for Context2
                    if context2_end > len(self.raw_data):
                        print(f"Warning: File too short for full Context2. "
                              f"Expected {CONTEXT_SIZE} bytes, "
                              f"got {len(self.raw_data) - context2_start} "
                              f"bytes")
                        self.context2 = self.raw_data[context2_start:]
                        if len(self.context2) < CONTEXT_SIZE:
                            print(f"Error: Context2 is too short "
                                  f"({len(self.context2)} bytes)")
                            return False
                    else:
                        # Extract Context2 (exactly 128 bytes)
                        self.context2 = self.raw_data[
                            context2_start:context2_end]
                else:
                    # ECB mode: only one context, no space separator expected
                    self.context2 = None
            else:
                # ECB mode: file ends after Context1
                self.context2 = None

            # Validate context sizes
            if len(self.context1) != CONTEXT_SIZE:
                print(f"Error: Context1 size is {len(self.context1)} "
                      f"bytes, expected {CONTEXT_SIZE} bytes")
                return False

            if self.context2 is not None and len(self.context2) != CONTEXT_SIZE:
                print(f"Error: Context2 size is {len(self.context2)} "
                      f"bytes, expected {CONTEXT_SIZE} bytes")
                return False

            return True

        except FileNotFoundError:
            print(f"Error: File {self.file_path} not found")
            return False
        except Exception as e:
            print(f"Error parsing file: {e}")
            import traceback
            traceback.print_exc()
            return False

    def get_mode(self) -> Optional[str]:
        """Get the encryption mode."""
        return self.mode

    def get_context1(self) -> Optional[bytes]:
        """Get the first context as binary data (128 bytes)."""
        return self.context1

    def get_context2(self) -> Optional[bytes]:
        """Get the second context as binary data.
        
        Returns 128 bytes for XTS mode, None for ECB mode.
        """
        return self.context2

    def get_context1_hex(self) -> Optional[str]:
        """Get the first context as hex string."""
        return self.context1.hex() if self.context1 else None

    def get_context2_hex(self) -> Optional[str]:
        """Get the second context as hex string."""
        return self.context2.hex() if self.context2 else None

    def is_xts_mode(self) -> bool:
        """Check if this is XTS mode (has two contexts)."""
        return self.context2 is not None

    def is_ecb_mode(self) -> bool:
        """Check if this is ECB mode (has one context)."""
        return self.context2 is None

    def print_summary(self):
        """Print a summary of the parsed data."""
        if not self.mode:
            print("No data parsed yet. Call parse() first.")
            return

        print("=" * 60)
        print("CRYPTO_CONTEXT.BIN Parser Results")
        print("=" * 60)
        print(f"File: {self.file_path}")
        print(f"File size: {len(self.raw_data)} bytes")
        print(f"Mode: {self.mode}")
        print(f"Mode type: {'XTS' if self.is_xts_mode() else 'ECB'}")
        print()

        if self.context1:
            print(f"Context1:")
            print(f"  Length: {len(self.context1)} bytes")
            print(f"  Hex: {self.get_context1_hex()}")
            print()

        if self.context2:
            print(f"Context2:")
            print(f"  Length: {len(self.context2)} bytes")
            print(f"  Hex: {self.get_context2_hex()}")
            print()

def main():
    """Main function to demonstrate the parser."""
    if len(sys.argv) < 2:
        # Default path
        file_path = ("/local/mnt/workspace/poovendh/customer_rootfs/"
                     "DUMP_ENC/CRYPTO_CONTEXT.BIN")
    else:
        file_path = sys.argv[1]

    if not os.path.exists(file_path):
        print(f"Error: File {file_path} does not exist")
        print("Usage: python3 crypto_context_parser.py "
              "[path_to_crypto_context.bin]")
        sys.exit(1)

    # Create parser and parse the file
    parser = CryptoContextParser(file_path)

    if not parser.parse():
        print("Failed to parse the file")
        sys.exit(1)

    # Print summary
    parser.print_summary()

if __name__ == "__main__":
    main()
