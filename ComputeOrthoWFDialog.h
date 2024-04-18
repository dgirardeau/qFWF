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

#ifndef COMPUTE_ORTHO_WF_DIALOG_HEADER
#define COMPUTE_ORTHO_WF_DIALOG_HEADER

#include <ui_ComputeOrthoWFDialog.h>

//Local
#include "FWFGrid.h"
#include "ccWaveform.h"

//Qt
#include <QDialog>
#include <QSharedPointer>

//qCC_db
#include <ccBBox.h>

//CCLib
#include <CCGeom.h>

class ccPointCloud;
class ccProgressDialog;
class ccBoundingBoxEditorDlg;
class ccMainAppInterface;
class ccWaveform;

class ComputeOrthoWFDialog : public QDialog, public Ui::ComputeOrthoWFDialog
{
	Q_OBJECT

public:
	
	//! Default constructor
	ComputeOrthoWFDialog(ccMainAppInterface* app, QWidget* parent = 0);

	//! Sets the associated cloud
	bool setCloud(ccPointCloud* cloud, ccProgressDialog* progressDialog = 0);

	static void ResetVlrsUsingOnlyOneDescriptor(const ccPointCloud* src, ccPointCloud* dst, const WaveformDescriptor& descriptor);

protected slots:

	void updateGridSize();
	void editBox();
	void invalidateGrid() { setCurrentGridValidity(false); }
	void exportToGridCloud();
	void exportToFWFCloud();
	void computeGrid();

protected: //methods

	//! Compute the grid size
	bool computeGridSize(Tuple3ui& size) const;

	//! Returns the selected interpolation method
	FWFGrid::InterpolationMethod getInterpolationMethod() const;

	//! Loads parameters from persistent settings
	void loadSettings();
	//! Save current parameters to persistent settings
	void saveSettings() const;

	//! Sets the current grid 'validity'
	void setCurrentGridValidity(bool state);

protected: //members

	//! Application interface
	ccMainAppInterface* m_app;
	
	//! Associated cloud
	ccPointCloud* m_cloud;

	//! FWF signal bounding-box (editor / manager)
	ccBoundingBoxEditorDlg* m_bbEditorDlg;

	//! Projected grid
	QSharedPointer<FWFGrid> m_grid;
};

#endif //COMPUTE_ORTHO_WF_DIALOG_HEADER
