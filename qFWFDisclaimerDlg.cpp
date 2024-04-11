//##########################################################################
//#                                                                        #
//#                       CLOUDCOMPARE PLUGIN: qFWF.                       #
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

#include "qFWFDisclaimerDlg.h"
#include "ui_qFWFDisclaimerDlg.h"

//qCC_plugins
#include <ccMainAppInterface.h>

//Qt
#include <QMainWindow>

bool qFWFDisclaimerDlg::s_disclaimerAccepted = false;

qFWFDisclaimerDlg::qFWFDisclaimerDlg(QWidget *parent)
    : QDialog(parent)
    , m_ui( new Ui::DisclaimerDialog )
{
    m_ui->setupUi( this );

    QString compilationInfo;
    compilationInfo += QStringLiteral("<br><i>Compiled with");

#if defined(_MSC_VER)
    compilationInfo += QStringLiteral(" MSVC %1 and").arg(_MSC_VER);
#endif

    compilationInfo += QStringLiteral(" Qt %1").arg(QT_VERSION_STR);
    compilationInfo += QStringLiteral("</i>");
}

qFWFDisclaimerDlg::~qFWFDisclaimerDlg()
{
    delete m_ui;
}

bool qFWFDisclaimerDlg::show(ccMainAppInterface *app)
{
    if ( !s_disclaimerAccepted )
    {
        //if the user "cancels" it, then he refuses the disclaimer
        s_disclaimerAccepted = qFWFDisclaimerDlg(app ? app->getMainWindow() : 0).exec();
    }

    return s_disclaimerAccepted;
}
