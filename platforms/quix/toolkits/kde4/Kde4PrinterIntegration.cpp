/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4PrinterIntegration.h"

#include "platforms/quix/toolkits/kde4/Kde4Utils.h"
#include "platforms/quix/toolkits/ToolkitPrinterHelper.h"

#include <QPrinter>
#include <QPrintDialog>
#include <QFile>
#include <kdeprintdialog.h>

Kde4PrinterIntegration::Kde4PrinterIntegration()
  : m_printer(0)
  , m_helper(0)
{
}

Kde4PrinterIntegration::~Kde4PrinterIntegration()
{
	delete m_printer;
	delete m_helper;
}

bool Kde4PrinterIntegration::Init(ToolkitPrinterHelper* helper)
{
	m_printer = new QPrinter;
	m_helper = helper;

	return m_printer && m_helper;
}

void Kde4PrinterIntegration::SetCaption(const char* caption)
{
	m_caption = QString::fromUtf8(caption);
}

bool Kde4PrinterIntegration::RunPrintDialog(X11Types::Window parent)
{
	QPrintDialog* dialog = KdePrint::createPrintDialog(m_printer, QList<QWidget*>());
	if (!dialog)
		return false;

	dialog->setWindowTitle(m_caption);
	Kde4Utils::SetResourceName(dialog, "printdialog");

	int result = Kde4Utils::RunDialog(dialog, parent);
	delete dialog;

	return (result == QDialog::Accepted);
}

bool Kde4PrinterIntegration::Print(const char* print_file_path, const char* print_job_name)
{
	if (!m_printer->outputFileName().isEmpty())
		return QFile::copy(QFile::decodeName(print_file_path), m_printer->outputFileName());

	if (!m_helper->SetPrinter(m_printer->printerName().toLocal8Bit()))
		return false;

	m_helper->SetCopies(m_printer->numCopies());

	return m_helper->PrintFile(print_file_path, print_job_name);
}

bool Kde4PrinterIntegration::PageShouldBePrinted(int page_number)
{
	return true;
}

bool Kde4PrinterIntegration::GetPrinterResolution(int& horizontal, int& vertical)
{
	vertical = horizontal = m_printer->resolution();
	return true;
}

bool Kde4PrinterIntegration::GetPrintScale(int& scale)
{
	scale = 100;
	return true;
}

bool Kde4PrinterIntegration::GetPaperSize(double& width, double& height)
{
#if QT_VERSION >= 0x040400
	QSizeF size = m_printer->paperSize(QPrinter::Inch);

	width = size.width();
	height = size.height();

	return true;
#else
	return false;
#endif
}

bool Kde4PrinterIntegration::GetMargins(double& top, double& left, double& bottom, double& right)
{
#if QT_VERSION >= 0x040400
	m_printer->getPageMargins(&left, &top, &right, &bottom, QPrinter::Inch);
	return true;
#else
	return false;
#endif
}
