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

#include "ComputeOrthoWFDialog.h"

//qCC
#include <ccBoundingBoxEditorDlg.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>

ComputeOrthoWFDialog::ComputeOrthoWFDialog(QWidget* parent/*=0*/)
	: QDialog(parent)
	, Ui::ComputeOrthoWFDialog()
	, m_cloud(0)
	, m_bbEditorDlg(0)
{
	setupUi(this);

	m_bbEditorDlg = new ccBoundingBoxEditorDlg(this);

	connect(hGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(vGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(editBoxToolButton, SIGNAL(clicked()), this, SLOT(editBox()));
	connect(computePushButton, SIGNAL(clicked()), this, SLOT(computeGrid()));
}

bool ComputeOrthoWFDialog::setCloud(ccPointCloud* cloud, ccProgressDialog* pDlg/*=0*/)
{
	m_cloud = cloud;
	m_bbEditorDlg->setBaseBBox(ccBBox());
	
	if (!m_cloud)
	{
		gridGroupBox->setEnabled(false);
		interpGroupBox->setEnabled(false);
		return false;
	}

	if (!m_bbEditorDlg)
	{
		assert(false);
		return false;
	}

	//determine the overall FWF bounding box
	{
		//update the progress dialog
		CCLib::NormalizedProgress nProgress(pDlg, static_cast<unsigned>(cloud->size()));
		if (pDlg)
		{
			pDlg->setWindowTitle("FWF");
			pDlg->setLabelText("Determine FWF bounding-box...");
			pDlg->show();
			QCoreApplication::processEvents();
		}

		const ccPointCloud::FWFDescriptorSet& descriptors = cloud->fwfDescriptors();

		ccBBox fwfBBox;
		for (unsigned pi = 0; pi < cloud->size(); ++pi)
		{
			if (!nProgress.oneStep())
			{
				//process cancelled by the user
				m_cloud = 0;
				return false;
			}

			const ccWaveform& w = cloud->fwfData()[pi];

			uint8_t descriptorID = w.descriptorID();
			if (descriptorID == 0 || !descriptors.contains(descriptorID))
			{
				//no valid descriptor
				continue;
			}

			const CCVector3* P = cloud->getPoint(pi);
			const WaveformDescriptor& d = descriptors[descriptorID];
			//we'll use the first and last points to increase the bounding-box
			CCVector3 Q0 = w.getSamplePos(0, *P, d);
			CCVector3 Q1 = w.getSamplePos(d.numberOfSamples - 1, *P, d);
			fwfBBox.add(Q0);
			fwfBBox.add(Q1);
		}

		m_bbEditorDlg->setBaseBBox(fwfBBox, false);
	}

	return true;
}

void ComputeOrthoWFDialog::editBox()
{
	if (m_bbEditorDlg && m_bbEditorDlg->exec())
	{
		updateGridSize();
	}
}

bool ComputeOrthoWFDialog::computeGridSize(Tuple3ui& size) const
{
	if (!m_bbEditorDlg || !m_bbEditorDlg->getBox().isValid())
	{
		assert(false);
		return false;
	}

	const ccBBox& box = m_bbEditorDlg->getBox();

	double hGridStep = hGridStepDoubleSpinBox->value();
	double vGridStep = vGridStepDoubleSpinBox->value();
	assert(hGridStep != 0 && vGridStep != 0);

	CCVector3d boxDiag = CCVector3d::fromArray(box.maxCorner().u) - CCVector3d::fromArray(box.minCorner().u);
	if (boxDiag.x <= 0 || boxDiag.y <= 0 || boxDiag.z <= 0)
	{
		return false;
	}

	//DGM: we now use the 'PixelIsArea' convention (the height value is computed at the grid cell center)
	size.x = 1 + static_cast<unsigned>(boxDiag.x / hGridStep + 0.5);
	size.y = 1 + static_cast<unsigned>(boxDiag.y / hGridStep + 0.5);
	size.z = 1 + static_cast<unsigned>(boxDiag.z / vGridStep + 0.5);

	return true;
}

void ComputeOrthoWFDialog::updateGridSize()
{
	Tuple3ui gridSize;
	if (!computeGridSize(gridSize))
	{
		gridWidthLabel->setText("invalid");
		return;
	}

	gridWidthLabel->setText(QString("%1 x %2 x %3").arg(gridSize.x).arg(gridSize.y).arg(gridSize.z));
}

void ComputeOrthoWFDialog::computeGrid()
{

}
