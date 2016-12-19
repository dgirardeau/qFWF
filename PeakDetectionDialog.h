//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qFWF                        #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 or later of the License.      #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                         COPYRIGHT: OSUR / CNRS                         #
//#                                                                        #
//##########################################################################

#ifndef PEAK_DETECTION_DIALOG_HEADER
#define PEAK_DETECTION_DIALOG_HEADER

#include <ui_PeakDetectionDialog.h>

//Qt
#include <QDialog>

class PeakDetectionDialog : public QDialog, public Ui::PeakDetectionDialog
{
public:
	
	//! Default constructor
	PeakDetectionDialog(QWidget* parent = 0)
		: QDialog(parent)
		, Ui::PeakDetectionDialog()
	{
		setupUi(this);
	}
};

#endif //PEAK_DETECTION_DIALOG_HEADER
