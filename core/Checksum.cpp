/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "Checksum.h"

#include <cstdio>

namespace Core
{
	namespace
	{
		//------------------------------------------------------------------
		// CRC-32/ISO-HDLC
		//------------------------------------------------------------------

		// Reflected polynomial. 0xEDB88320 is 0x04C11DB7 bit-reversed, which is
		// what a table-driven reflected implementation uses.
		const uint32_t CRC32_POLY = 0xEDB88320u;

		// Built once on first use rather than written out as 256 literals, so the
		// polynomial above is the only magic number to check.
		struct SCrc32Table
		{
			uint32_t _Entries[256];

			SCrc32Table()
			{
				for (uint32_t i = 0; i < 256; ++i)
				{
					uint32_t c = i;
					for (int k = 0; k < 8; ++k)
					{
						c = (c & 1u) ? (CRC32_POLY ^ (c >> 1)) : (c >> 1);
					}
					_Entries[i] = c;
				}
			}
		};

		const SCrc32Table& Crc32Table()
		{
			static const SCrc32Table table;
			return table;
		}

		//------------------------------------------------------------------
		// SHA-1 (FIPS 180-4)
		//------------------------------------------------------------------

		inline uint32_t RotateLeft(uint32_t value, unsigned int bits)
		{
			return (value << bits) | (value >> (32 - bits));
		}

		void Sha1ProcessBlock(const unsigned char* pBlock, uint32_t h[5])
		{
			uint32_t w[80];
			for (int i = 0; i < 16; ++i)
			{
				w[i] = (static_cast<uint32_t>(pBlock[i * 4]) << 24)
					| (static_cast<uint32_t>(pBlock[i * 4 + 1]) << 16)
					| (static_cast<uint32_t>(pBlock[i * 4 + 2]) << 8)
					| static_cast<uint32_t>(pBlock[i * 4 + 3]);
			}
			for (int i = 16; i < 80; ++i)
			{
				w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
			}

			uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
			for (int i = 0; i < 80; ++i)
			{
				uint32_t f = 0;
				uint32_t k = 0;
				if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999u; }
				else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1u; }
				else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDCu; }
				else             { f = b ^ c ^ d;                     k = 0xCA62C1D6u; }

				const uint32_t temp = RotateLeft(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = RotateLeft(b, 30);
				b = a;
				a = temp;
			}
			h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
		}
	}

	uint32_t Crc32(const void* pData, size_t nBytes)
	{
		const unsigned char* p = static_cast<const unsigned char*>(pData);
		const SCrc32Table& table = Crc32Table();
		uint32_t crc = 0xFFFFFFFFu;
		for (size_t i = 0; i < nBytes; ++i)
		{
			crc = table._Entries[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
		}
		return crc ^ 0xFFFFFFFFu;
	}

	uint32_t Crc32(const std::string& strData)
	{
		return Crc32(strData.data(), strData.size());
	}

	std::string Sha1Hex(const void* pData, size_t nBytes)
	{
		uint32_t h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
		const unsigned char* p = static_cast<const unsigned char*>(pData);

		size_t offset = 0;
		for (; nBytes - offset >= 64; offset += 64)
		{
			Sha1ProcessBlock(p + offset, h);
		}

		// Tail: remaining bytes, 0x80, zero padding, then the 64-bit big-endian
		// bit length. Needs two blocks when the remainder leaves no room for the
		// length field.
		unsigned char tail[128] = { 0 };
		const size_t nRemaining = nBytes - offset;
		for (size_t i = 0; i < nRemaining; ++i)
		{
			tail[i] = p[offset + i];
		}
		tail[nRemaining] = 0x80;

		const size_t nTailBlocks = (nRemaining >= 56) ? 2u : 1u;
		const uint64_t nBitLength = static_cast<uint64_t>(nBytes) * 8u;
		const size_t nLengthAt = nTailBlocks * 64u - 8u;
		for (int i = 0; i < 8; ++i)
		{
			tail[nLengthAt + i] =
				static_cast<unsigned char>((nBitLength >> (56 - 8 * i)) & 0xFFu);
		}
		for (size_t i = 0; i < nTailBlocks; ++i)
		{
			Sha1ProcessBlock(tail + i * 64, h);
		}

		char buf[41];
		for (int i = 0; i < 5; ++i)
		{
			std::snprintf(buf + i * 8, 9, "%08x", h[i]);
		}
		return std::string(buf, 40);
	}

	std::string Sha1Hex(const std::string& strData)
	{
		return Sha1Hex(strData.data(), strData.size());
	}
}
