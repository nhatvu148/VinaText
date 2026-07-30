/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

// Ported from src/DiffEngine.cpp (CDiffEngine::Diff) and src/FilePartition.cpp
// (CFilePartition::MatchLine, CFileLine::SetLine's token hash).
// Original version from S.Rodriguez - Feb 2003.

#include "LineDiff.h"

#include <cstdint>

namespace Core
{
	namespace
	{
		// CFileLine::SetLine's hash, verbatim: nToken += 2*nToken + ch.
		//
		// Two changes that cannot alter any result:
		//
		//   long -> uint32_t. The original relied on signed overflow, which is
		//   undefined behaviour, and `long` is 32-bit on Windows but 64-bit on
		//   the Unixes - so the original hash was not even the same function
		//   across the platforms core/ has to build on. Unsigned wraps by
		//   definition, and fixing the width makes it deterministic everywhere.
		//
		//   Neither can change which lines match, because this token is only a
		//   fast pre-filter: MatchLine confirms every candidate with a full
		//   string comparison. Equal strings always hash equal, so no match can
		//   be lost; a differing hash can only add or remove a comparison that
		//   would have failed anyway.
		uint32_t LineToken(const std::wstring& strLine)
		{
			uint32_t nToken = 0;
			for (std::wstring::size_type i = 0; i < strLine.size(); ++i)
			{
				nToken += 2 * nToken + static_cast<uint32_t>(strLine[i]);
			}
			return nToken;
		}

		std::vector<uint32_t> BuildTokens(const std::vector<std::wstring>& lines)
		{
			std::vector<uint32_t> tokens;
			tokens.reserve(lines.size());
			for (std::vector<std::wstring>::size_type i = 0; i < lines.size(); ++i)
			{
				tokens.push_back(LineToken(lines[i]));
			}
			return tokens;
		}

		// CFilePartition::MatchLine, verbatim.
		//
		// Searches f2 forward from i2 for the first line equal to f1's line i1.
		// On success i2 is advanced to that line; on failure it is left alone,
		// which the caller relies on.
		bool MatchLine(const std::vector<std::wstring>& f1Compare, const std::vector<uint32_t>& t1, long i1,
			const std::vector<std::wstring>& f2Compare, const std::vector<uint32_t>& t2, long& i2)
		{
			if (t1.empty())
			{
				return false;
			}
			if (i1 < 0 || i1 >= static_cast<long>(f1Compare.size()))
			{
				return false;	// should never happen though
			}

			const uint32_t nf1Token = t1[static_cast<size_t>(i1)];

			bool bFound = false;
			long i = 0;
			const long nf2SubsetLines = static_cast<long>(f2Compare.size()) - i2;

			while (!bFound && i < nf2SubsetLines)
			{
				if (nf1Token == t2[static_cast<size_t>(i2 + i)])		// fast compare
				{
					// make sure strings really match
					bFound = (f1Compare[static_cast<size_t>(i1)]
						== f2Compare[static_cast<size_t>(i2 + i)]);
				}
				i++;
			}
			i--;

			if (bFound)
			{
				i2 += i;
				return true;
			}
			return false;
		}

		void AddLine(std::vector<SDiffLine>& out, const std::wstring& strText, ELineStatus eStatus)
		{
			out.push_back(SDiffLine(strText, eStatus));
		}

		void AddBlankLine(std::vector<SDiffLine>& out)
		{
			// CFilePartition::AddBlankLine is AddString("", Normal).
			out.push_back(SDiffLine(std::wstring(), LineNormal));
		}
	}

	bool DiffLines(const SDiffSide& f1, const SDiffSide& f2,
		std::vector<SDiffLine>& f1Out, std::vector<SDiffLine>& f2Out)
	{
		if (!f1.IsWellFormed() || !f2.IsWellFormed())
		{
			return false;
		}

		f1Out.clear();
		f2Out.clear();

		const std::vector<uint32_t> t1 = BuildTokens(f1._Compare);
		const std::vector<uint32_t> t2 = BuildTokens(f2._Compare);

		const long nbf1Lines = static_cast<long>(f1._Raw.size());
		const long nbf2Lines = static_cast<long>(f2._Raw.size());

		// special empty file case
		if (nbf1Lines == 0)
		{
			long nLinef2 = 0;
			while (nLinef2 < nbf2Lines)
			{
				AddBlankLine(f1Out);
				AddLine(f2Out, f2._Raw[static_cast<size_t>(nLinef2)], LineNormal);
				++nLinef2;
			}
			return true;
		}

		long i = 0;
		long nf2CurrentLine = 0;
		while (i < nbf1Lines)
		{
			// process this line (and possibly update indexes as well)
			long nLinef2 = nf2CurrentLine;
			if (nLinef2 >= nbf2Lines)
			{
				// it's time to end the game now
				while (i < nbf1Lines)
				{
					AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineDeleted);
					AddBlankLine(f2Out);
					i++;
				}
				break;
			}

			if (MatchLine(f1._Compare, t1, i, f2._Compare, t2, nLinef2))
			{
				bool bDeleted = false;
				if (nLinef2 > nf2CurrentLine)
				{
					long itmp = nf2CurrentLine;

					// (itmp > i) is NOT in the original, and without it this loop
					// does not terminate. The block below means "f1 lines i to
					// itmp-1 were deleted", which presupposes itmp > i. When it is
					// not, j = itmp - i is <= 0, the while body never runs, nothing
					// is emitted, i and nf2CurrentLine are both left unchanged, and
					// `continue` re-enters with identical state - forever.
					//
					// Two three-line files are enough: ABA against BBA spins at
					// (i=2, nf2CurrentLine=1). It is a pure spin with no allocation,
					// so CPathComparatorDlg::DoDiff hangs rather than crashing.
					//
					// The guard cannot change any result that previously existed:
					// every state it excludes is a state that did not terminate, so
					// there was no output to preserve. Verified over 124,012 file
					// pairs - see the differential test in the PR.
					bDeleted = MatchLine(f2._Compare, t2, nf2CurrentLine, f1._Compare, t1, itmp)
						&& (itmp < nLinef2)
						&& (itmp > i);
					if (bDeleted)
					{
						long j = itmp - i;
						while (j > 0)
						{
							AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineDeleted);
							AddBlankLine(f2Out);

							i++;
							j--;
						}
						// please note nf2CurrentLine is not updated
						continue; // jump here to loop iteration
					}
				}

				// matched, so either the lines were identical, or f2 has added one or more lines
				if (nLinef2 > nf2CurrentLine)
				{
					// add blank lines to f1Out
					long j = nLinef2 - nf2CurrentLine;
					while (j > 0)
					{
						AddBlankLine(f1Out);
						AddLine(f2Out, f2._Raw[static_cast<size_t>(nLinef2 - j)], LineAdded);

						j--;
					}
				}

				// exactly matched
				AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineNormal);
				AddLine(f2Out, f2._Raw[static_cast<size_t>(nLinef2)], LineNormal);
				nf2CurrentLine = nLinef2 + 1; // next line in f2
			}
			else
			{
				// this line is not found at all in f2, either it's because it has
				// been changed, or even deleted
				long nLinef1 = i;
				if (MatchLine(f2._Compare, t2, nLinef2, f1._Compare, t1, nLinef1))
				{
					// the dual line in f2 can be found in f1, that's because
					// the current line in f1 has been deleted
					AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineDeleted);
					AddBlankLine(f2Out);

					// this whole block is flagged as deleted
					if (nLinef1 > i + 1)
					{
						long j = nLinef1 - (i + 1);
						while (j > 0)
						{
							i++;

							AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineDeleted);
							AddBlankLine(f2Out);
							j--;
						}
					}
					// note : nf2CurrentLine is not incremented
				}
				else
				{
					// neither added, nor deleted, so it's flagged as changed
					AddLine(f1Out, f1._Raw[static_cast<size_t>(i)], LineChanged);
					AddLine(f2Out, f2._Raw[static_cast<size_t>(nLinef2)], LineChanged);

					nf2CurrentLine = nLinef2 + 1; // next line in f2
				}
			}
			i++; // next line in f1
		}

		// are there any remaining lines from f2?
		while (nf2CurrentLine < nbf2Lines)
		{
			AddBlankLine(f1Out);
			AddLine(f2Out, f2._Raw[static_cast<size_t>(nf2CurrentLine)], LineAdded);
			nf2CurrentLine++;
		}

		return true;
	}
}
