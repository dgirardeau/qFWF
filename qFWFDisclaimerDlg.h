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
//#            COPYRIGHT: UNIVERSITE EUROPEENNE DE BRETAGNE                #
//#                                                                        #
//##########################################################################

#ifndef Q_FWF_DISCLAIMER_DLG_H
#define Q_FWF_DISCLAIMER_DLG_H

#include <QDialog>

class ccMainAppInterface;

namespace Ui {
class DisclaimerDialog;
}

//! Dialog for displaying the M3C2/UEB disclaimer
class qFWFDisclaimerDlg : public QDialog
{
public:
    qFWFDisclaimerDlg(QWidget* parent = nullptr);
    ~qFWFDisclaimerDlg();

    static bool show(ccMainAppInterface* app);

private:
    //whether disclaimer has already been displayed (and accepted) or not
    static bool s_disclaimerAccepted;

    Ui::DisclaimerDialog* m_ui;
};

#endif // Q_FWF_DISCLAIMER_DLG_H
