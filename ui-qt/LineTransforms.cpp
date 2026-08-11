/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "LineTransforms.h"

#include <QCoreApplication>
#include <QObject>

#include <memory>

namespace
{
	using FTransform = CEditorWidget::FLineTransform;
	const FTransform REFUSED;		// an empty function: the input was rejected

	QString Tr(const char* sz)
	{
		return QCoreApplication::translate("LineTransforms", sz);
	}

	// "[Error] Inputs are empty!" is the MFC's message for all seventeen.
	bool Empty(const QString& str, QString& strErrorOut)
	{
		if (str.isEmpty())
		{
			strErrorOut = Tr("Inputs are empty.");
			return true;
		}
		return false;
	}

	// The MFC splits X on commas and requires EVERY piece to be present in the
	// line - AppUtils::SplitterCString then a count == size test. Transcribed,
	// including that an empty piece matches everything.
	QStringList Pieces(const QString& str)
	{
		return str.split(QLatin1Char(','));
	}

	bool ContainsAll(const QString& strLine, const QStringList& pieces)
	{
		for (const QString& strPiece : pieces)
		{
			if (!strLine.contains(strPiece))
			{
				return false;
			}
		}
		return true;
	}
}

QString LineTransforms::ToRoman(int nValue)
{
	// AppUtils::DecimalToRomanNumerals, same tables and same greedy walk.
	static const int values[] = { 1,4,5,9,10,40,50,90,100,400,500,900,1000 };
	static const char* const symbols[] =
		{ "I","IV","V","IX","X","XL","L","XC","C","CD","D","CM","M" };
	QString strResult;
	for (int i = 12; i >= 0 && nValue > 0; --i)
	{
		int nCount = nValue / values[i];
		nValue %= values[i];
		while (nCount-- > 0)
		{
			strResult += QLatin1String(symbols[i]);
		}
	}
	return strResult;
}

const std::vector<LineTransforms::SCommand>& LineTransforms::All()
{
	static const std::vector<SCommand> commands = {

	//----------------------------------------------------------------------
	// Lines kept or dropped. The primitive's std::nullopt is what "remove"
	// means here - the line never reaches the rebuilt text.
	//----------------------------------------------------------------------
	{ "Remove Lines Containing...",
	  { Tr("Remove line contain word..."), Tr("Words (comma separated):"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QStringList pieces = Pieces(dlg.Value1());
		  return [pieces](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  return ContainsAll(strLine, pieces) ? std::nullopt
												  : std::optional<QString>(strLine);
		  };
	  } },

	{ "Remove Lines Not Containing...",
	  { Tr("Remove line not contain word..."), Tr("Words (comma separated):"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QStringList pieces = Pieces(dlg.Value1());
		  return [pieces](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  return ContainsAll(strLine, pieces) ? std::optional<QString>(strLine)
												  : std::nullopt;
		  };
	  } },

	//----------------------------------------------------------------------
	// Insert at a line edge.
	//----------------------------------------------------------------------
	{ "Insert At Line Start...",
	  { Tr("Insert Word At Begin Line"), Tr("Text:"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strText = dlg.Value1();
		  return [strText](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  return strText + strLine;
		  };
	  } },

	{ "Insert At Line End...",
	  { Tr("Insert Word At End Line"), Tr("Text:"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strText = dlg.Value1();
		  return [strText](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  return strLine + strText;
		  };
	  } },

	//----------------------------------------------------------------------
	// The four index inserters. All four differ only in how a counter is
	// rendered, and all four use " - " as the separator, which is the MFC's.
	//----------------------------------------------------------------------
	{ "Insert Line Numbers (prefix)...",
	  { Tr("Add prefix number for each line..."), Tr("Start at:"), QStringLiteral("0"),
		{}, {}, {}, true },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const int nStart = dlg.Value1().toInt();
		  return [nStart](const QString& strLine, const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  return QString::number(nStart + ctx._Index) + QStringLiteral(" - ") + strLine;
		  };
	  } },

	{ "Insert Line Numbers (suffix)...",
	  { Tr("Add suffix number for each line..."), Tr("Start at:"), QStringLiteral("0"),
		{}, {}, {}, true },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const int nStart = dlg.Value1().toInt();
		  return [nStart](const QString& strLine, const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  // No " - " on the suffix form, which is the MFC's asymmetry:
			  // the prefix version separates, this one abuts.
			  return strLine + QString::number(nStart + ctx._Index);
		  };
	  } },

	{ "Insert Alphabet Index...",
	  { Tr("Add prefix alphabet for each line..."), Tr("Start at:"), QStringLiteral("A"),
		{}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  const QString strStart = dlg.Value1().trimmed();
		  if (strStart.size() != 1 || !strStart.at(0).isLetter())
		  {
			  strError = Tr("Enter a single letter to start from.");
			  return REFUSED;
		  }
		  const ushort nFirst = strStart.at(0).unicode();
		  return [nFirst](const QString& strLine, const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  // Plain increment, so past 'Z' it runs into '[' exactly as the
			  // MFC's nIndexChar++ does. Reproduced rather than wrapped: a
			  // wrap would need a scheme the original never chose.
			  return QString(QChar(static_cast<ushort>(nFirst + ctx._Index)))
				  + QStringLiteral(" - ") + strLine;
		  };
	  } },

	{ "Insert Roman Numerals...",
	  { Tr("Add prefix roman numerals for each line..."), Tr("Start at:"),
		QStringLiteral("1"), {}, {}, {}, true },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const int nStart = dlg.Value1().toInt();
		  // The MFC's own bounds. Roman numerals have no zero and its table
		  // stops at M, so 3999 is the largest it can render.
		  if (nStart < 1 || nStart > 3999)
		  {
			  strError = Tr("Start must be between 1 and 3999.");
			  return REFUSED;
		  }
		  return [nStart](const QString& strLine, const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  return ToRoman(nStart + ctx._Index) + QStringLiteral(" - ") + strLine;
		  };
	  } },

	//----------------------------------------------------------------------
	// Word-relative edits.
	//----------------------------------------------------------------------
	{ "Remove Before Word...",
	  { Tr("Remove Before Word In Line"), Tr("Before word:"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strWord = dlg.Value1();
		  return [strWord](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  const int nAt = strLine.indexOf(strWord);
			  return nAt < 0 ? strLine : strLine.mid(nAt);
		  };
	  } },

	{ "Remove After Word...",
	  { Tr("Remove After Word In Line"), Tr("After word:"), {}, {}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strWord = dlg.Value1();
		  return [strWord](const QString& strLine, const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  const int nAt = strLine.indexOf(strWord);
			  return nAt < 0 ? strLine : strLine.left(nAt + strWord.size());
		  };
	  } },

	{ "Insert Before Word...",
	  { Tr("Insert Before Word In Line"), Tr("Before word:"), {},
		Tr("Insert:"), {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError) || Empty(dlg.Value2(), strError))
		  {
			  return REFUSED;
		  }
		  const QString strWord = dlg.Value1();
		  const QString strWhat = dlg.Value2();
		  return [strWord, strWhat](const QString& strLine, const CEditorWidget::SLineContext&)
			  -> std::optional<QString>
		  {
			  const int nAt = strLine.indexOf(strWord);
			  return nAt < 0 ? strLine
							 : strLine.left(nAt) + strWhat + strLine.mid(nAt);
		  };
	  } },

	{ "Insert After Word...",
	  { Tr("Insert After Word In Line"), Tr("After word:"), {},
		Tr("Insert:"), {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError) || Empty(dlg.Value2(), strError))
		  {
			  return REFUSED;
		  }
		  const QString strWord = dlg.Value1();
		  const QString strWhat = dlg.Value2();
		  return [strWord, strWhat](const QString& strLine, const CEditorWidget::SLineContext&)
			  -> std::optional<QString>
		  {
			  const int nAt = strLine.indexOf(strWord);
			  if (nAt < 0) { return strLine; }
			  const int nEnd = nAt + strWord.size();
			  return strLine.left(nEnd) + strWhat + strLine.mid(nEnd);
		  };
	  } },

	//----------------------------------------------------------------------
	// Column-relative edits.
	//----------------------------------------------------------------------
	{ "Remove From Column X To Y...",
	  { Tr("Remove From Position X To Y In Line"), Tr("From:"), QStringLiteral("0"),
		Tr("To:"), {}, Tr("Count from the end of the line"), true },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError) || Empty(dlg.Value2(), strError))
		  {
			  return REFUSED;
		  }
		  const int nFrom = dlg.Value1().toInt();
		  const int nTo = dlg.Value2().toInt();
		  if (nFrom < 0 || nTo < 0 || nTo < nFrom)
		  {
			  strError = Tr("Inputs are invalid.");
			  return REFUSED;
		  }
		  const bool bFromEnd = dlg.IsChecked();
		  return [nFrom, nTo, bFromEnd](const QString& strLine, const CEditorWidget::SLineContext&)
			  -> std::optional<QString>
		  {
			  // The MFC reverses the line, cuts, and reverses back. Same
			  // result, without building two extra strings.
			  if (bFromEnd)
			  {
				  const int nLen = strLine.size();
				  const int nCutTo = qBound(0, nLen - nFrom, nLen);
				  const int nCutFrom = qBound(0, nLen - nTo, nCutTo);
				  return strLine.left(nCutFrom) + strLine.mid(nCutTo);
			  }
			  const int nLen = strLine.size();
			  const int nA = qBound(0, nFrom, nLen);
			  const int nB = qBound(nA, nTo, nLen);
			  return strLine.left(nA) + strLine.mid(nB);
		  };
	  } },

	{ "Insert At Column...",
	  { Tr("Insert At Position In Line"), Tr("Column:"), QStringLiteral("0"),
		Tr("Insert:"), {}, Tr("Count from the end of the line"), true },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError) || Empty(dlg.Value2(), strError))
		  {
			  return REFUSED;
		  }
		  const int nAt = dlg.Value1().toInt();
		  const QString strWhat = dlg.Value2();
		  const bool bFromEnd = dlg.IsChecked();
		  return [nAt, strWhat, bFromEnd](const QString& strLine, const CEditorWidget::SLineContext&)
			  -> std::optional<QString>
		  {
			  const int nLen = strLine.size();
			  const int nPos = qBound(0, bFromEnd ? nLen - nAt : nAt, nLen);
			  return strLine.left(nPos) + strWhat + strLine.mid(nPos);
		  };
	  } },

	{ "Remove Between Characters...",
	  { Tr("Remove From Character X To Y In Line"), Tr("From character:"), {},
		Tr("To character:"), {}, Tr("Search from the end of the line"), false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError) || Empty(dlg.Value2(), strError))
		  {
			  return REFUSED;
		  }
		  if (dlg.Value1().size() != 1 || dlg.Value2().size() != 1)
		  {
			  strError = Tr("Inputs must each be a single character.");
			  return REFUSED;
		  }
		  const QChar chFrom = dlg.Value1().at(0);
		  const QChar chTo = dlg.Value2().at(0);
		  const bool bFromEnd = dlg.IsChecked();
		  return [chFrom, chTo, bFromEnd](const QString& strLine,
			  const CEditorWidget::SLineContext&) -> std::optional<QString>
		  {
			  // TWO DEFECTS NOT REPRODUCED, both in the original's version:
			  //
			  //   1. Its "from the end" branch calls ReverseFind(m_strFromX[0])
			  //      TWICE - the second should be m_strToY. So that mode
			  //      ignores the second input entirely and searches for the
			  //      same character for both ends.
			  //   2. Neither branch guards a character that is not present.
			  //      Find returns -1, and Mid(0, -1 + 1) + Mid(-1) then
			  //      DUPLICATES the prefix onto the whole line.
			  //
			  // A line missing either character is left alone here, which is
			  // what the word-relative transforms above already do.
			  const int nFrom = bFromEnd ? strLine.lastIndexOf(chFrom)
										 : strLine.indexOf(chFrom);
			  const int nTo = bFromEnd ? strLine.lastIndexOf(chTo)
									   : strLine.indexOf(chTo);
			  if (nFrom < 0 || nTo < 0 || nTo < nFrom)
			  {
				  return strLine;
			  }
			  // Keeps both characters and removes what lies strictly between.
			  return strLine.left(nFrom + 1) + strLine.mid(nTo);
		  };
	  } },

	//----------------------------------------------------------------------
	// The two that change how many lines there are.
	//----------------------------------------------------------------------
	{ "Split Lines On Delimiter...",
	  { Tr("Split Line With Delimiter"), Tr("Delimiter:"), QStringLiteral(" "),
		{}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strDelim = dlg.Value1();
		  return [strDelim](const QString& strLine,
			  const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  // One line becomes several, by returning a string that already
			  // contains THIS DOCUMENT'S line ending. That is why the context
			  // carries the EOL: joining with a hard-coded "\n" would convert
			  // a CRLF file one line at a time.
			  QStringList pieces = strLine.split(strDelim);
			  pieces.removeAll(QString());		// the MFC skips empty pieces
			  return pieces.join(ctx._Eol);
		  };
	  } },

	{ "Join Lines With Delimiter...",
	  { Tr("Join Line With Delimiter"), Tr("Delimiter:"), QStringLiteral(" "),
		{}, {}, {}, false },
	  [](const CTransformDialog& dlg, QString& strError) -> FTransform
	  {
		  if (Empty(dlg.Value1(), strError)) { return REFUSED; }
		  const QString strDelim = dlg.Value1();
		  auto pAccumulated = std::make_shared<QString>();
		  auto pAny = std::make_shared<bool>(false);
		  return [strDelim, pAccumulated, pAny](const QString& strLine,
			  const CEditorWidget::SLineContext& ctx) -> std::optional<QString>
		  {
			  // Many lines become one: accumulate, emit nothing, and hand the
			  // whole thing back on the LAST line of the scope - which is why
			  // the context carries _Total. Blank lines are skipped, as the
			  // MFC's Trim().IsEmpty() check does.
			  //
			  // No trailing delimiter. The MFC appends one after every line
			  // including the last, so its joined line ends in stray
			  // punctuation; that is visible junk rather than a behaviour.
			  if (!strLine.trimmed().isEmpty())
			  {
				  if (*pAny) { *pAccumulated += strDelim; }
				  *pAccumulated += strLine;
				  *pAny = true;
			  }
			  if (ctx._Index + 1 < ctx._Total)
			  {
				  return std::nullopt;
			  }
			  return *pAny ? std::optional<QString>(*pAccumulated) : std::nullopt;
		  };
	  } },
	};
	return commands;
}
