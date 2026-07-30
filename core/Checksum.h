/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// CRC-32 and SHA-1, replacing boost/crc.hpp and boost/uuid/sha1.hpp.
//
// core/ code: UI-free and portable, no MFC, no Win32, no Qt.
//
// These are the two Boost uses in the codebase where a replacement must produce
// BYTE-IDENTICAL output - they feed the editor's checksum display, and a
// different-but-plausible answer is worse than no answer. Both are fully
// specified algorithms with published test vectors, so equivalence is provable
// rather than assumed:
//
//   CRC-32/ISO-HDLC   RFC 1952 (gzip), the variant boost::crc_32_type implements:
//                     polynomial 0x04C11DB7 reflected to 0xEDB88320, init and
//                     final XOR 0xFFFFFFFF, input and output reflected.
//   SHA-1             FIPS 180-4, the variant boost::uuids::detail::sha1
//                     implements.
//
// core/tests/TestChecksum.cpp checks both against the standard vectors.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Core
{
	// CRC-32/ISO-HDLC. Matches boost::crc_32_type and gzip/zlib.
	//   Crc32("123456789") == 0xCBF43926
	uint32_t Crc32(const void* pData, size_t nBytes);
	uint32_t Crc32(const std::string& strData);

	// SHA-1 (FIPS 180-4), returned as 40 lowercase hex characters.
	//   Sha1Hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d"
	std::string Sha1Hex(const void* pData, size_t nBytes);
	std::string Sha1Hex(const std::string& strData);
}
