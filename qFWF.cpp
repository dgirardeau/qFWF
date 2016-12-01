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

#include "qFWF.h"

//local

//CCLib

//qCC_db
#include <ccPointCloud.h>

//Qt
#include <QtGui>
#include <QtCore>
#include <QApplication>

qCanupoPlugin::qCanupoPlugin(QObject* parent/*=0*/)
	: QObject(parent)
	, m_convertWFToCloudAction(0)
	, m_computeOrthoWF(0)
{
}

void qCanupoPlugin::onNewSelection(const ccHObject::Container& selectedEntities)
{
	if (m_convertWFToCloudAction)
	{
		//convert waveform to cloud: only one point cloud with waveform
		m_convertWFToCloudAction->setEnabled(selectedEntities.size() == 1 && selectedEntities[0]->isA(CC_TYPES::POINT_CLOUD) && static_cast<ccPointCloud*>(selectedEntities[0])->hasFWF());
	}

	if (m_computeOrthoWF)
	{
		//convert waveform to cloud: only one point cloud with waveform
		m_computeOrthoWF->setEnabled(selectedEntities.size() == 1 && selectedEntities[0]->isA(CC_TYPES::POINT_CLOUD) && static_cast<ccPointCloud*>(selectedEntities[0])->hasFWF());
	}

	m_selectedEntities = selectedEntities;
}

void qCanupoPlugin::getActions(QActionGroup& group)
{
	if (!m_convertWFToCloudAction)
	{
		m_convertWFToCloudAction = new QAction("Waveform to cloud", this);
		m_convertWFToCloudAction->setToolTip("Convert waveform to cloud");
		m_convertWFToCloudAction->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qFWF/icon.png")));
		connect(m_convertWFToCloudAction, SIGNAL(triggered()), this, SLOT(doTrainAction()));
	}
	group.addAction(m_convertWFToCloudAction);

	if (!m_computeOrthoWF)
	{
		m_computeOrthoWF = new QAction("Compute ortho-waveform", this);
		m_computeOrthoWF->setToolTip("Projects the waveforms in a grid-like structure to generate ortho-waveforms");
		m_computeOrthoWF->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qFWF/icon.png")));
		connect(m_computeOrthoWF, SIGNAL(triggered()), this, SLOT(doClassifyAction()));
	}
	group.addAction(m_computeOrthoWF);
}

ccPointCloud* qCanupoPlugin::getCloudWithFWF()
{
	if (!m_app)
	{
		assert(false);
		return nullptr;
	}

	if (m_selectedEntities.empty() || !m_selectedEntities.front()->isA(CC_TYPES::POINT_CLOUD))
	{
		m_app->dispToConsole("Select one and only one point cloud!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return nullptr;
	}

	ccPointCloud* cloud = static_cast<ccPointCloud*>(m_selectedEntities.front());
	if (!cloud->hasFWF())
	{
		m_app->dispToConsole("Cloud has no associated FWF data", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return nullptr;
	}

	return cloud;
}

void qCanupoPlugin::doConvertWFToCloudAction()
{
	ccPointCloud* cloud = getCloudWithFWF();
	if (!cloud)
	{
		return;
	}
	assert(m_app && cloud->hasFWF()); //see getCloudWithFWF

	//TDO
}

void qCanupoPlugin::doComputeOrthoWF()
{
	ccPointCloud* cloud = getCloudWithFWF();
	if (!cloud)
	{
		return;
	}
	assert(m_app && cloud->hasFWF()); //see getCloudWithFWF
}


QIcon qCanupoPlugin::getIcon() const
{
	return QIcon(QString::fromUtf8(":/CC/plugin/qCanupoPlugin/iconClassify.png"));
}
