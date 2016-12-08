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
#include <ccMainAppInterface.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

//Qt
#include <QSettings>

ComputeOrthoWFDialog::ComputeOrthoWFDialog(ccMainAppInterface* app, QWidget* parent/*=0*/)
	: QDialog(parent)
	, Ui::ComputeOrthoWFDialog()
	, m_app(app)
	, m_cloud(0)
	, m_bbEditorDlg(0)
{
	setupUi(this);

	m_bbEditorDlg = new ccBoundingBoxEditorDlg(this);

	connect(hGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(vGridStepDoubleSpinBox,	SIGNAL(valueChanged(double)), this, SLOT(updateGridSize()));
	connect(editBoxToolButton, SIGNAL(clicked()), this, SLOT(editBox()));
	connect(computePushButton, SIGNAL(clicked()), this, SLOT(computeGrid()));
	connect(buttonBox, &QDialogButtonBox::accepted, [&]() { saveSettings(); accept(); });

	connect(hGridStepDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(vGridStepDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(radiusDoubleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(invalidateGrid()));
	connect(interpMethodComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(invalidateGrid()));

	connect(computePushButton, SIGNAL(clicked()), this, SLOT(computeGrid()));
	connect(exportCloudPushButton, SIGNAL(clicked()), this, SLOT(exportToFWFCloud()));
	connect(exportGridPushButton, SIGNAL(clicked()), this, SLOT(exportToGridCloud()));

	setCurrentGridValidity(false);
}

void ComputeOrthoWFDialog::loadSettings()
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("ComputeOrthoWFDialog");
	{
		double hGridStep = settings.value("hGridStep", hGridStepDoubleSpinBox->value()).toDouble();
		double vGridStep = settings.value("vGridStep", vGridStepDoubleSpinBox->value()).toDouble();
		double radius = settings.value("radius", radiusDoubleSpinBox->value()).toDouble();
		int interpMethod = settings.value("interpMethod", interpMethodComboBox->currentIndex()).toInt();

		hGridStepDoubleSpinBox->setValue(hGridStep);
		vGridStepDoubleSpinBox->setValue(vGridStep);
		radiusDoubleSpinBox->setValue(radius);
		interpMethodComboBox->setCurrentIndex(interpMethod);
	}
	settings.endGroup();
	settings.endGroup();
}

void ComputeOrthoWFDialog::saveSettings() const
{
	QSettings settings;
	settings.beginGroup("FWF_plugin");
	settings.beginGroup("ComputeOrthoWFDialog");
	{
		settings.setValue("hGridStep", hGridStepDoubleSpinBox->value());
		settings.setValue("vGridStep", vGridStepDoubleSpinBox->value());
		settings.setValue("radius", radiusDoubleSpinBox->value());
		settings.setValue("interpMethod", interpMethodComboBox->currentIndex());
	}
	settings.endGroup();
	settings.endGroup();
}

bool ComputeOrthoWFDialog::setCloud(ccPointCloud* cloud, ccProgressDialog* pDlg/*=0*/)
{
	m_cloud = cloud;
	m_bbEditorDlg->setBaseBBox(ccBBox());
	
	if (!m_cloud)
	{
		gridGroupBox->setEnabled(false);
		interpGroupBox->setEnabled(false);
		buttonFrame->setEnabled(false);
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

	gridGroupBox->setEnabled(true);
	interpGroupBox->setEnabled(true);
	buttonFrame->setEnabled(true);

	loadSettings();
	updateGridSize();

	return true;
}

void ComputeOrthoWFDialog::editBox()
{
	if (m_bbEditorDlg && m_bbEditorDlg->exec())
	{
		updateGridSize();
		setCurrentGridValidity(false);
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
		buttonFrame->setEnabled(false);
		return;
	}
	
	buttonFrame->setEnabled(true);
	gridWidthLabel->setText(QString("%1 x %2 x %3").arg(gridSize.x).arg(gridSize.y).arg(gridSize.z));
}

ComputeOrthoWFDialog::InterpolationMethod ComputeOrthoWFDialog::getInterpolationMethod() const
{
	switch (interpMethodComboBox->currentIndex())
	{
	case 0:
		return LINEAR;
	case 1:
		return INV_SQUARE;
	case 2:
		return INV_EXP;
	}

	assert(false);
	return LINEAR;
}

void ComputeOrthoWFDialog::setCurrentGridValidity(bool state)
{
	if (state)
	{
		assert(m_grid);
	}
	else
	{
		m_grid.clear();
	}

	buttonFrame->setEnabled(state);
}

void ComputeOrthoWFDialog::computeGrid()
{
	if (!m_cloud)
	{
		assert(false);
		return;
	}

	Tuple3ui gridSize;
	if (!computeGridSize(gridSize))
	{
		assert(false);
		return;
	}
	assert(m_bbEditorDlg && m_bbEditorDlg->getBox().isValid());

	gridInfoLabel->setText("Initializing the grid");

	if (m_grid)
	{
		m_grid.clear();
	}
	setCurrentGridValidity(false);

	//create a new grid
	m_grid.reset(new FWFGrid);

	CCVector3d steps(	hGridStepDoubleSpinBox->value(),
						hGridStepDoubleSpinBox->value(),
						vGridStepDoubleSpinBox->value() );

	const ccBBox& box = m_bbEditorDlg->getBox();

	CCVector3d minCorner = CCVector3d::fromArray(box.minCorner().u);

	//init the grid
	if (!m_grid->init(gridSize, steps, minCorner))
	{
		if (m_app)
		{
			m_app->dispToConsole("Failed to initialize the grid", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			gridInfoLabel->setText("Failed to initialize the grid");

		return;
		}
	}

	gridInfoLabel->setText("Computing grid");

	//now fill it
	{
		//progress dialog
		ccProgressDialog pDlg(m_app ? m_app->getMainWindow() : 0);
		CCLib::NormalizedProgress nProgress(&pDlg, static_cast<unsigned>(m_cloud->size()));
		pDlg.setWindowTitle("FWF");
		pDlg.setLabelText("Projecting the waveforms...");
		pDlg.show();
		QCoreApplication::processEvents();

		const ccPointCloud::FWFDescriptorSet& descriptors = m_cloud->fwfDescriptors();

		double radius = radiusDoubleSpinBox->value();
		double squareRadius = radius * radius;
		double sigma = radius / 3.0;
		double expFactor = -1.0 / (2.0 * sigma * sigma); // = - 9/2 * squareRadius
		
		int kernelSize = static_cast<int>(ceil(radius / steps.x));

		InterpolationMethod interpMethod = getInterpolationMethod();

		for (unsigned pi = 0; pi < m_cloud->size(); ++pi)
		{
			if (!nProgress.oneStep())
			{
				//process cancelled by the user
				m_grid.clear();
				return;
			}

			const ccWaveform& w = m_cloud->fwfData()[pi];

			uint8_t descriptorID = w.descriptorID();
			if (descriptorID == 0 || !descriptors.contains(descriptorID))
			{
				//no valid descriptor
				continue;
			}

			const CCVector3* P = m_cloud->getPoint(pi);
			const WaveformDescriptor& d = descriptors[descriptorID];
			
			for (uint32_t si = 0; si < d.numberOfSamples; ++si)
			{
				double amplitude = w.getSample(si, d);
				if (amplitude == 0)
				{
					continue;
				}
				
				CCVector3 Q = w.getSamplePos(si, *P, d);
				
				//compute the eqiuvalent position in the grid
				Tuple3i voxelPos = m_grid->computeCellPos(Q);
				if (!m_grid->inbounds(voxelPos))
				{
					assert(false);
					continue;
				}

				//get the right slice
				FWFGrid::Slice& slice = m_grid->getSlice(voxelPos.z);

				//now add the contribution of this point to its neighbors
				int minX = std::max(0, voxelPos.x - kernelSize);
				int maxX = std::min(static_cast<int>(gridSize.x) - 1, voxelPos.x + kernelSize);
				int minY = std::max(0, voxelPos.y - kernelSize);
				int maxY = std::min(static_cast<int>(gridSize.y) - 1, voxelPos.y + kernelSize);
				for (int i = minX; i <= maxX; ++i)
				{
					double dx = Q.x - (minCorner.x + (i + 0.5) * steps.x);
					double dx2 = dx * dx;
					for (int j = minY; j <= maxY; ++j)
					{
						double dy = Q.y - (minCorner.y + (j + 0.5) * steps.y);
						double r2 = dx2 + dy*dy;
						if (r2 <= squareRadius)
						{
							FWFGrid::Cell& cell = slice[j * gridSize.x + i];

							double weight = 1.0;
							switch (interpMethod)
							{
							case LINEAR:
								weight = (radius - sqrt(r2)) / radius; //weight proportional to the distance to the center
								break;
							case INV_SQUARE:
								weight += (squareRadius - r2) / squareRadius; //weight proportional to the squared distance to the center
								break;
							case INV_EXP:
								weight += exp(expFactor * r2); //weight proportional to the exponential of the inversed distance to the center
								break;
							default:
								assert(false);
							}

							cell.add(amplitude, weight);
						}
					}
				}
			}
		}
	}

	double maxVal = m_grid->computeMaxVal();
	minAmpDoubleSpinBox->setMaximum(maxVal);
	gridInfoLabel->setText(QString("Grid computed (max amplitude: %1)").arg(maxVal));

	setCurrentGridValidity(true);
}

ccPointCloud* ComputeOrthoWFDialog::ConvertGridToGridCloud(	const FWFGrid& grid,
															double minVal/*=0*/,
															ccMainAppInterface* app/*=0*/)
{
	//count the number of valid cells
	unsigned validCellCount = 0;
	{
		for (unsigned z = 0; z < grid.size().z; ++z)
		{
			const FWFGrid::Slice& slice = grid.getSlice(z);
			for (const FWFGrid::Cell& cell : slice)
			{
				if (cell.weightSum != 0 && cell.mean() > minVal)
				{
					++validCellCount;
				}
			}
		}
	}

	if (validCellCount == 0)
	{
		if (app)
			app->dispToConsole("Grid has no valid cell?!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return 0;
	}

	//create the cloud
	ccPointCloud* cloud = 0;
	{
		cloud = new ccPointCloud("fwf_grid");
		ccScalarField* amplitudes = new ccScalarField("Mean amplitude");
		ccScalarField* stdDev = new ccScalarField("Amplitude std. dev.");

		cloud->addScalarField(amplitudes);
		cloud->addScalarField(stdDev);

		if (!cloud->reserve(validCellCount))
		{
			//not enough memory
			if (app)
				app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			delete cloud;
			return 0;
		}

		for (unsigned z = 0; z < grid.size().z; ++z)
		{
			const FWFGrid::Slice& slice = grid.getSlice(z);
			const FWFGrid::Cell* _cell = &(slice[0]);

			Tuple3i currentVoxel(0, 0, static_cast<int>(z));
			for (unsigned y = 0; y < grid.size().y; ++y)
			{
				currentVoxel.y = static_cast<int>(y);
				for (unsigned x = 0; x < grid.size().x; ++x, ++_cell)
				{
					if (_cell->weightSum == 0)
					{
						continue;
					}
					
					double meanVal = _cell->mean();
					if (meanVal > minVal)
					{
						currentVoxel.x = static_cast<int>(x);
						CCVector3d C = grid.computeCellCenter(currentVoxel);

						cloud->addPoint(CCVector3::fromArray(C.u));
						amplitudes->addElement(meanVal);
						stdDev->addElement(_cell->stdDev());
					}
				}
			}
		}

		amplitudes->computeMinAndMax();
		stdDev->computeMinAndMax();

		cloud->setCurrentDisplayedScalarField(0);
		cloud->showSF(true);
	}

	return cloud;
}

void ComputeOrthoWFDialog::exportToGridCloud()
{
	if (!m_app || !m_grid)
	{
		assert(false);
		return;
	}


	ccPointCloud* cloud = ComputeOrthoWFDialog::ConvertGridToGridCloud(*m_grid, minAmpDoubleSpinBox->value(), m_app);

	if (cloud)
	{
		cloud->setName(m_cloud->getName() + ".fwf_grid");
		if (m_cloud->getParent())
		{
			m_cloud->getParent()->addChild(cloud);
		}
		m_app->addToDB(cloud);
	}
}

void ComputeOrthoWFDialog::exportToFWFCloud()
{
	if (!m_app || !m_grid)
	{
		assert(false);
		return;
	}

}
