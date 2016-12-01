//##########################################################################
//#                                                                        #
//#                     CLOUDCOMPARE PLUGIN: qCANUPO                       #
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

#ifndef CONVERT_WF_TO_CLOUD_DIALOG_HEADER
#define CONVERT_WF_TO_CLOUD_DIALOG_HEADER

#include <ui_ConvertWFToCloudDialog.h>

//Qt
#include <QDialog>

class ConvertWFToCloudDialog : public QDialog, public Ui::ConvertWFToCloudDialog
{
public:
	
	//! Default constructor
	ConvertWFToCloudDialog(QWidget* parent = 0)
		: QDialog(parent)
		, Ui::ConvertWFToCloudDialog()
	{
		setupUi(this);
	}
};

#endif //CONVERT_WF_TO_CLOUD_DIALOG_HEADER
