/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Proves core/Checksum.{h,cpp} is byte-identical to what it replaces.
//
// These two functions feed the editor's checksum display, so a
// different-but-plausible answer is worse than no answer. Both algorithms are
// fully specified with published test vectors, so equivalence with
// boost::crc_32_type and boost::uuids::detail::sha1 is provable rather than
// assumed - any conforming implementation must produce these exact values.
//
// Build and run (one line):
//   c++ -std=c++11 -I core core/Checksum.cpp core/tests/TestChecksum.cpp -o testchecksum && ./testchecksum

#include "Checksum.h"

#include <cstdio>
#include <iostream>
#include <string>

namespace
{
	int g_Failures = 0;
	int g_Checks = 0;

	void CheckHex(const std::string& actual, const std::string& expected, const std::string& what)
	{
		++g_Checks;
		if (actual != expected)
		{
			++g_Failures;
			std::cout << "  FAIL: " << what << "\n        actual   = " << actual
				<< "\n        expected = " << expected << "\n";
		}
	}

	void CheckCrc(uint32_t actual, uint32_t expected, const std::string& what)
	{
		++g_Checks;
		if (actual != expected)
		{
			++g_Failures;
			char a[16], e[16];
			std::snprintf(a, sizeof a, "0x%08X", actual);
			std::snprintf(e, sizeof e, "0x%08X", expected);
			std::cout << "  FAIL: " << what << "\n        actual   = " << a
				<< "\n        expected = " << e << "\n";
		}
	}
}

int main()
{
	//----------------------------------------------------------------------
	// CRC-32/ISO-HDLC — the variant boost::crc_32_type implements
	//----------------------------------------------------------------------

	// The check value every CRC-32/ISO-HDLC implementation is defined against.
	CheckCrc(Core::Crc32(std::string("123456789")), 0xCBF43926u,
		"CRC-32 check value for \"123456789\"");

	CheckCrc(Core::Crc32(std::string("")), 0x00000000u, "CRC-32 of the empty string");
	CheckCrc(Core::Crc32(std::string("a")), 0xE8B7BE43u, "CRC-32 of \"a\"");
	CheckCrc(Core::Crc32(std::string("abc")), 0x352441C2u, "CRC-32 of \"abc\"");
	CheckCrc(Core::Crc32(std::string("The quick brown fox jumps over the lazy dog")),
		0x414FA339u, "CRC-32 of the pangram");

	// Embedded NUL: must hash the bytes, not stop at a terminator.
	{
		const char raw[] = { 'a', '\0', 'b' };
		CheckCrc(Core::Crc32(raw, sizeof raw), Core::Crc32(std::string(raw, sizeof raw)),
			"CRC-32 pointer and string overloads agree across an embedded NUL");
	}

	//----------------------------------------------------------------------
	// SHA-1 (FIPS 180-4) — the variant boost::uuids::detail::sha1 implements
	//----------------------------------------------------------------------

	CheckHex(Core::Sha1Hex(std::string("abc")),
		"a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 of \"abc\" (FIPS 180-4)");

	CheckHex(Core::Sha1Hex(std::string("")),
		"da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 of the empty string");

	// 56 bytes: the padding boundary. The 0x80 byte leaves no room for the
	// 64-bit length, so this must spill into a second block.
	CheckHex(Core::Sha1Hex(std::string(
		"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
		"84983e441c3bd26ebaae4aa1f95129e5e54670f1", "SHA-1 of the 56-byte FIPS vector");

	// 55 bytes: one byte under the boundary, so exactly one tail block.
	CheckHex(Core::Sha1Hex(std::string(55, 'a')),
		"c1c8bbdc22796e28c0e15163d20899b65621d65a", "SHA-1 of 55 'a' (single tail block)");

	// 64 bytes: exactly one full block, so the tail is padding only.
	CheckHex(Core::Sha1Hex(std::string(64, 'a')),
		"0098ba824b5c16427bd7a1122a5a442a25ec644d", "SHA-1 of 64 'a' (exact block multiple)");

	// 1,000,000 'a' — the long FIPS vector, exercising many blocks.
	CheckHex(Core::Sha1Hex(std::string(1000000, 'a')),
		"34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA-1 of 1e6 'a' (FIPS long vector)");

	std::cout << "\n" << (g_Checks - g_Failures) << "/" << g_Checks << " checks passed\n";
	if (g_Failures != 0)
	{
		std::cout << g_Failures << " FAILED\n";
		return 1;
	}
	std::cout << "OK\n";
	return 0;
}
