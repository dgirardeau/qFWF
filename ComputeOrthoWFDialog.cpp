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

//local
#include "OrthoWFTools.h"
#include "FWFPeakDetectorNaive.h"
#include "FWFPeakDetectorNLSGauss.h"

//qCC
#include <ccBoundingBoxEditorDlg.h>
#include <ccMainAppInterface.h>

//qCC_db
#include <ccPointCloud.h>
#include <ccProgressDialog.h>
#include <ccScalarField.h>

//Qt
#include <QSettings>

#include <ui_ComputeOrthoWFToCloudDialog.h>

//dialog for conversion to a FWF cloud
class ComputeOrthoWFToCloudDialog : public QDialog, public Ui::ComputeOrthoWFToCloudDialog
{
public:
	ComputeOrthoWFToCloudDialog(QWidget* parent = 0)
		: QDialog(parent)
		, Ui::ComputeOrthoWFToCloudDialog()
	{
		setupUi(this);
	}
};

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
	buttonFrame->setEnabled(false);

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

			const ccWaveformProxy& w = cloud->waveformProxy(pi);
			if (!w.isValid())
			{
				//no valid descriptor
				continue;
			}

			const CCVector3* P = cloud->getPoint(pi);
			//we'll use the first and last points to increase the bounding-box
			CCVector3 Q0 = w.getSamplePos(0, *P);
			CCVector3 Q1 = w.getSamplePos(w.numberOfSamples() - 1, *P);
			fwfBBox.add(Q0);
			fwfBBox.add(Q1);
		}

		m_bbEditorDlg->setBaseBBox(fwfBBox, false);
	}

	gridGroupBox->setEnabled(true);
	interpGroupBox->setEnabled(true);

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
		computePushButton->setEnabled(false);
	}
	else
	{
		gridWidthLabel->setText(QString("%1 x %2 x %3").arg(gridSize.x).arg(gridSize.y).arg(gridSize.z));
		computePushButton->setEnabled(true);
	}
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

			const ccWaveformProxy& w = m_cloud->waveformProxy(pi);
			if (!w.isValid())
			{
				//no valid descriptor
				continue;
			}

			const CCVector3* P = m_cloud->getPoint(pi);
			
			for (uint32_t si = 0; si < w.numberOfSamples(); ++si)
			{
				double amplitude = w.getSample(si);
				if (amplitude == 0)
				{
					continue;
				}
				
				CCVector3 Q = w.getSamplePos(si, *P);
				
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

void ComputeOrthoWFDialog::exportToGridCloud()
{
	if (!m_app || !m_grid)
	{
		assert(false);
		return;
	}

	ccPointCloud* cloud = OrthoWFTools::ConvertGridToGridCloud(*m_grid, minAmpDoubleSpinBox->value(), m_app);
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

	double rangeMin = 0, rangeMax = 0;
	if (!m_grid->computeRange(rangeMin, rangeMax))
	{
		if (m_app)
			m_app->dispToConsole("Grid has no valid cell?!", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	//mean peak value
	static double s_minPeakVal = rangeMax + 1.0;
	if (s_minPeakVal > rangeMax)
	{
		s_minPeakVal = rangeMin + (rangeMax - rangeMin) / 5;
	}
	static int s_peakDetectorIndex = 1;
	static int s_maxReturnCount = 2;
	static int s_bpsMode = 1;
	static int s_numberOfSamples = 64;

	ComputeOrthoWFToCloudDialog owf2cloudDlg(this);

	//load parameters
	owf2cloudDlg.minAmplitudeDoubleSpinBox->setRange(rangeMin, rangeMax);
	owf2cloudDlg.minAmplitudeDoubleSpinBox->setToolTip(QString("Range: [%1 ; %2]").arg(rangeMin).arg(rangeMax));
	owf2cloudDlg.minAmplitudeDoubleSpinBox->setValue(s_minPeakVal);
	owf2cloudDlg.echoDetectorComboBox->addItem("Naive");
	owf2cloudDlg.echoDetectorComboBox->addItem("NLS_Gauss");
	owf2cloudDlg.echoDetectorComboBox->setCurrentIndex(s_peakDetectorIndex);
	owf2cloudDlg.maxReturnCountSpinBox->setValue(s_maxReturnCount);
	owf2cloudDlg.bpsComboBox->setCurrentIndex(s_bpsMode);
	owf2cloudDlg.sampleCountSpinBox->setValue(s_numberOfSamples);
	
	if (!owf2cloudDlg.exec())
	{
		//process cancelled by the user
		return;
	}

	//save parameters
	s_minPeakVal = owf2cloudDlg.minAmplitudeDoubleSpinBox->value();
	s_peakDetectorIndex = owf2cloudDlg.echoDetectorComboBox->currentIndex();
	s_maxReturnCount = owf2cloudDlg.maxReturnCountSpinBox->value();
	s_bpsMode = owf2cloudDlg.bpsComboBox->currentIndex();
	s_numberOfSamples = owf2cloudDlg.sampleCountSpinBox->value();

	//peak detector
	FWFPeakDetectorInterface* detector = 0;
	switch (s_peakDetectorIndex)
	{
	case 0:
		detector = new FWFPeakDetectorNaive;
		break;
	case 1:
		detector = new FWFPeakDetectorNLSGauss;
		break;
	default:
		assert(false);
		if (m_app)
			m_app->dispToConsole("Internal error: invalid peak detector", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	WaveformDescriptor descriptor;
	switch (s_bpsMode)
	{
	case 0:
		descriptor.bitsPerSample = 8;
		break;
	case 1:
		descriptor.bitsPerSample = 16;
		break;
	case 2:
		descriptor.bitsPerSample = 32;
		break;
	default:
		assert(false);
		descriptor.bitsPerSample = 16;
		break;
	}

	descriptor.numberOfSamples = s_numberOfSamples;

	//set the gain and offset automatically
	descriptor.digitizerOffset = rangeMin;
	if (descriptor.digitizerOffset > 0)
	{
		//auto-shift to 0
		descriptor.digitizerOffset = 0;
	}
	descriptor.digitizerGain = (rangeMax - descriptor.digitizerOffset) / (1 << descriptor.bitsPerSample);

	//compute the right sampling rate
	//1) find the first valid waveform
	for (const ccWaveform& w : m_cloud->waveforms())
	{
		if (w.descriptorID() != 0 && m_cloud->fwfDescriptors().contains(w.descriptorID()))
		{
			WaveformDescriptor& d = m_cloud->fwfDescriptors()[w.descriptorID()];

			double sampleStep = w.beamDir().normd();
			double gridStep = m_grid->steps().z;

			//descriptor.samplingRate_ps = static_cast<uint32_t>(gridStep * d.samplingRate_ps / sampleStep);
			descriptor.samplingRate_ps = 1000;
			break;
		}
	}

	ccPointCloud* cloud = OrthoWFTools::ConvertGridToFWFCloud(	*m_grid,
																detector,
																s_maxReturnCount,
																descriptor,
																s_minPeakVal,
																m_app);

	delete detector;
	detector = 0;

	if (cloud)
	{
		cloud->setName(m_cloud->getName() + ".fwf");
		if (m_cloud->getParent())
		{
			m_cloud->getParent()->addChild(cloud);
		}
		m_app->addToDB(cloud);
	}
}
