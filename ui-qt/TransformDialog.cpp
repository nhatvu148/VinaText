/*#*******************************************************************************
# COPYRIGHT NOTES
# ---------------
# This is a part of VinaText Project
# Copyright(C) - free open source - vinadevs
# This source code can be used, distributed or modified under MIT license
#*******************************************************************************/

#include "TransformDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

CTransformDialog::CTransformDialog(const SSpec& spec, QWidget* pParent)
	: QDialog(pParent)
{
	setWindowTitle(spec._Title);

	QFormLayout* pForm = new QFormLayout();
	m_pField1 = new QLineEdit(spec._Value1, this);
	m_pField1->setClearButtonEnabled(true);
	pForm->addRow(spec._Label1, m_pField1);

	if (!spec._Label2.isEmpty())
	{
		m_pField2 = new QLineEdit(spec._Value2, this);
		m_pField2->setClearButtonEnabled(true);
		pForm->addRow(spec._Label2, m_pField2);
	}
	if (!spec._CheckLabel.isEmpty())
	{
		m_pCheck = new QCheckBox(spec._CheckLabel, this);
		pForm->addRow(QString(), m_pCheck);
	}

	if (spec._DigitsOnly)
	{
		// ES_NUMBER, as the MFC's position fields carry - digits only, and
		// deliberately NOT a bounded QIntValidator. A widget's range is a guide
		// for new input, not a licence to rewrite what was typed (PR #45).
		auto* pDigits = new QRegularExpressionValidator(
			QRegularExpression(QStringLiteral("[0-9]*")), this);
		m_pField1->setValidator(pDigits);
		if (m_pField2 != nullptr)
		{
			m_pField2->setValidator(pDigits);
		}
	}

	QDialogButtonBox* pButtons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->addLayout(pForm);
	pLayout->addWidget(pButtons);

	m_pField1->setFocus();
	m_pField1->selectAll();
}

QString CTransformDialog::Value1() const
{
	return m_pField1->text();
}

QString CTransformDialog::Value2() const
{
	return m_pField2 == nullptr ? QString() : m_pField2->text();
}

bool CTransformDialog::IsChecked() const
{
	return m_pCheck != nullptr && m_pCheck->isChecked();
}

void CTransformDialog::SetValuesForTest(const QString& str1, const QString& str2,
	bool bChecked)
{
	m_pField1->setText(str1);
	if (m_pField2 != nullptr)
	{
		m_pField2->setText(str2);
	}
	if (m_pCheck != nullptr)
	{
		m_pCheck->setChecked(bChecked);
	}
}
