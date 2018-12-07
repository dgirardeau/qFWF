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

#ifndef Q_FWF_PLUGIN_HEADER
#define Q_FWF_PLUGIN_HEADER

//qCC
#include <ccStdPluginInterface.h>

//qCC_db
#include <ccHObject.h>

class ccPointCloud;

//! (LAS) Full Waveform plugin
class qFWF : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
		Q_INTERFACES(ccStdPluginInterface)
		Q_PLUGIN_METADATA(IID "cccorp.cloudcompare.plugin.qFWF" FILE "info.json")

public:

	//! Default constructor
	qFWF(QObject* parent = nullptr);

	//inherited from ccStdPluginInterface
	void onNewSelection(const ccHObject::Container& selectedEntities) override;
	virtual QList<QAction*> getActions() override;
	virtual void registerCommands(ccCommandLineInterface* cmd) override;

protected slots:

	void doConvertWFToCloudAction();
	void doComputeOrthoWF();
	void doExtractNewPeaks();
	void doTestPeakExtraction();

protected: //methods

	//! Returns the selected cloud with FWF from the current selection (if any)
	ccPointCloud* getCloudWithFWF();

protected: //members

	//! Convert waveform to cloud action
	QAction* m_convertWFToCloudAction;
	//! Compute ortho waveform
	QAction* m_computeOrthoWFAction;
	//! Extracts peaks from a pointcloud with existing FWF data
	QAction* m_extractNewPeaksAction;
	//! Test of the peak extraction module
	QAction* m_testPeakExtractionAction;

	//! Currently selected entities
	ccHObject::Container m_selectedEntities;
};

#endif //Q_FWF_PLUGIN_HEADER
