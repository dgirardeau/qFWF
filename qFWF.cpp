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

#include "qFWF.h"

//local
#include "ConvertWFToCloudDialog.h"
#include "ComputeOrthoWFDialog.h"

//qCC_db
#include <ccPointCloud.h>
#include <ccScalarField.h>
#include <ccProgressDialog.h>

//qCC_io
#include <LASFields.h>

//Qt
#include <QtGui>
#include <QtCore>
#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>

//for peak extraction test
#include <QInputDialog>
#include "FWFPeakDetectorNLSGauss.h"

qFWF::qFWF(QObject* parent/*=0*/)
	: QObject(parent)
	, m_convertWFToCloudAction(0)
	, m_computeOrthoWF(0)
	, m_testPeakExtraction(0)
{
}

void qFWF::onNewSelection(const ccHObject::Container& selectedEntities)
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

	if (m_testPeakExtraction)
	{
		//we need one point cloud with waveform
		m_testPeakExtraction->setEnabled(selectedEntities.size() == 1 && selectedEntities[0]->isA(CC_TYPES::POINT_CLOUD) && static_cast<ccPointCloud*>(selectedEntities[0])->hasFWF());
	}

	m_selectedEntities = selectedEntities;
}

void qFWF::getActions(QActionGroup& group)
{
	if (!m_convertWFToCloudAction)
	{
		m_convertWFToCloudAction = new QAction("Waveform to cloud", this);
		m_convertWFToCloudAction->setToolTip("Convert waveform to cloud");
		m_convertWFToCloudAction->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qFWF/wave2.png")));
		connect(m_convertWFToCloudAction, SIGNAL(triggered()), this, SLOT(doConvertWFToCloudAction()));
	}
	group.addAction(m_convertWFToCloudAction);

	if (!m_computeOrthoWF)
	{
		m_computeOrthoWF = new QAction("Compute ortho-waveform", this);
		m_computeOrthoWF->setToolTip("Projects the waveforms in a grid-like structure to generate ortho-waveforms");
		m_computeOrthoWF->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qFWF/wave_grid.png")));
		connect(m_computeOrthoWF, SIGNAL(triggered()), this, SLOT(doComputeOrthoWF()));
	}
	group.addAction(m_computeOrthoWF);

	if (!m_testPeakExtraction)
	{
		m_testPeakExtraction = new QAction("Test peak extraction", this);
		m_testPeakExtraction->setToolTip("Extracts peaks from a single waveform");
		m_testPeakExtraction->setIcon(QIcon(QString::fromUtf8(":/CC/plugin/qFWF/wave.png")));
		connect(m_testPeakExtraction, SIGNAL(triggered()), this, SLOT(doTestPeakExtraction()));
	}
	group.addAction(m_testPeakExtraction);
}

ccPointCloud* qFWF::getCloudWithFWF()
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

void qFWF::doConvertWFToCloudAction()
{
	ccPointCloud* cloud = getCloudWithFWF();
	if (!cloud)
	{
		return;
	}
	assert(m_app && cloud->hasFWF()); //see getCloudWithFWF

	//get the overal signal amplitude
	double overallMin, overallMax;
	ccProgressDialog pDlg(m_app->getMainWindow());
	pDlg.setAutoClose(false);
	if (!cloud->computeFWFAmplitude(overallMin, overallMax, &pDlg))
	{
		m_app->dispToConsole("Cloud has no valid FWF data", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}

	//check if the cloud has a 'return number' scalar field
	int rnSFIndex = cloud->getScalarFieldIndexByName(LAS_FIELD_NAMES[LAS_RETURN_NUMBER]);

	//init dialog
	ConvertWFToCloudDialog dialog;
	static bool s_useOnlyFirstEcho = true;
	dialog.onlyFirstEchoCheckBox->setChecked(s_useOnlyFirstEcho);
	dialog.onlyFirstEchoCheckBox->setEnabled(rnSFIndex >= 0);

	static double s_minAplitude = 0;
	dialog.minAmplitudeDoubleSpinBox->setRange(overallMin, overallMax);
	dialog.minAmplitudeDoubleSpinBox->setToolTip(QString("Range: [%1 ; %2]").arg(overallMin).arg(overallMax));
	dialog.minAmplitudeDoubleSpinBox->setValue(s_minAplitude);

	if (!dialog.exec())
	{
		//process cancelled by the user
		return;
	}

	s_useOnlyFirstEcho = dialog.onlyFirstEchoCheckBox->isChecked();
	s_minAplitude = dialog.minAmplitudeDoubleSpinBox->value();

	//shall we use the return numbers?
	CCLib::ScalarField* returnNumbers = 0;
	if (s_useOnlyFirstEcho && rnSFIndex >= 0)
	{
		returnNumbers = cloud->getScalarField(rnSFIndex);
		assert(returnNumbers);
	}

	//shortcut
	const ccPointCloud::FWFDescriptorSet& descriptors = cloud->fwfDescriptors();

	//count the number of candidate values
	size_t valueCount = 0;
	{
		//update the progress dialog
		CCLib::NormalizedProgress nProgress(&pDlg, static_cast<unsigned>(cloud->size()));
		pDlg.setValue(0);
		pDlg.setWindowTitle("FWF");
		pDlg.setLabelText("Counting candidates...");
		QCoreApplication::processEvents();

		assert(cloud->waveforms().size() == cloud->size());
		for (unsigned pi = 0; pi < cloud->size(); ++pi)
		{
			if (!nProgress.oneStep())
			{
				//process cancelled by the user
				return;
			}

			if (returnNumbers && returnNumbers->getValue(pi) > 1)
			{
				//skip secondary echos
				continue;
			}

			const ccWaveformProxy& w = cloud->waveformProxy(pi);
			if (!w.isValid())
			{
				//no valid descriptor
				continue;
			}

			for (uint32_t i = 0; i < w.numberOfSamples(); ++i)
			{
				if (w.getSample(i) >= s_minAplitude)
				{
					++valueCount;
				}
			}
		}
	}

	if (valueCount == 0)
	{
		m_app->dispToConsole("No point generated (check the parameters)", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
		return;
	}
	//if the output cloud might be too big, ask the user for confirmation
	if (	valueCount > 20000000
		&&	QMessageBox::question
			(
				m_app->getMainWindow(),
				"Samples",
				QString("%1 points will be created. Ready?").arg(QLocale(QLocale::English).toString(valueCount)),
				QMessageBox::Yes,
				QMessageBox::No
			) == QMessageBox::No
		)
	{
		return;
	}

	//now we can create the cloud
	{
		//update the progress dialog
		CCLib::NormalizedProgress nProgress(&pDlg, static_cast<unsigned>(cloud->size()));
		pDlg.setValue(0);
		pDlg.setWindowTitle("FWF");
		pDlg.setLabelText("Exporting candidates...");
		QCoreApplication::processEvents();

		ccPointCloud* newCloud = new ccPointCloud(cloud->getName() + QString(".fwf(>%1)").arg(s_minAplitude));
		ccScalarField* amplitudes = new ccScalarField("Amplitudes");
		newCloud->addScalarField(amplitudes);

		if (!newCloud->reserve(static_cast<unsigned>(valueCount)))
		{
			m_app->dispToConsole("Not enough memory", ccMainAppInterface::ERR_CONSOLE_MESSAGE);
			delete newCloud;
			return;
		}

		for (unsigned pi = 0; pi < cloud->size(); ++pi)
		{
			if (!nProgress.oneStep())
			{
				//process cancelled by the user
				delete newCloud;
				return;
			}

			if (returnNumbers && returnNumbers->getValue(pi) > 1)
			{
				//skip secondary echos
				continue;
			}

			const ccWaveformProxy& w = cloud->waveformProxy(pi);
			if (!w.isValid())
			{
				//no valid descriptor
				continue;
			}

			const CCVector3* P = cloud->getPoint(pi);
			for (uint32_t i = 0; i < w.numberOfSamples(); ++i)
			{
				double amplitude = w.getSample(i);
				if (amplitude >= s_minAplitude)
				{
					CCVector3 Q = w.getSamplePos(i, *P);
					newCloud->addPoint(Q);
					amplitudes->addElement(amplitude);
				}
			}
		}

		amplitudes->computeMinAndMax();
		newCloud->setCurrentDisplayedScalarField(0);
		newCloud->showSF(true);
		if (cloud->getParent())
		{
			cloud->getParent()->addChild(newCloud);
		}

		m_app->addToDB(newCloud);
	}
}

void qFWF::doComputeOrthoWF()
{
	ccPointCloud* cloud = getCloudWithFWF();
	if (!cloud)
	{
		return;
	}
	assert(m_app && cloud->hasFWF()); //see getCloudWithFWF

	ComputeOrthoWFDialog dlg(m_app, m_app->getMainWindow());
	if (dlg.setCloud(cloud))
	{
		dlg.exec();
	}
	else
	{
		//something went wrong?!
	}
}

void qFWF::doTestPeakExtraction()
{
	ccPointCloud* cloud = getCloudWithFWF();
	if (!cloud)
	{
		return;
	}
	assert(m_app && cloud->hasFWF()); //see getCloudWithFWF

	bool ok = true;
	int pointIndex = QInputDialog::getInt(m_app->getMainWindow(), "Waveform index", "Waveform / point", 0, 0, cloud->size() - 1, 100, &ok);
	if (!ok)
	{
		//cancelled by the user
		return;
	}

	//get the waveform
	const ccWaveformProxy& w = cloud->waveformProxy(pointIndex);
	if (!w.isValid())
	{
		//no valid descriptor
		return;
	}

	try
	{
		//then get the values
		std::vector<double> values;
		if (!w.decodeSamples(values))
		{
			//not enough memory?
			m_app->dispToConsole("Failed to decode the waveform values", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			return;
		}

		//setup the detector
		FWFPeakDetectorNLSGauss detector;
		FWFPeakDetectorNLSGauss::BaseParameters baseParams;
		{
			baseParams.maxPeakCount = 16;
			baseParams.minIntensity = 20;
		}
		FWFPeakDetectorNLSGauss::SpecificParameters specifParams;
		{
			//Gaussian filter 'sigma' (in ns)
			double sigma_ns = 1;
			//Equivalent in terms of 'samples'
			specifParams.smoothWaveformSigma = static_cast<double>(sigma_ns * 1000) / w.descriptor().samplingRate_ps;
		}
		detector.setSpecificParameters(specifParams);

		std::vector<FWFPeakDetectorInterface::Peak> peaks;
		if (!detector.detectPeaks(peaks, values, baseParams))
		{
			//not enough memory?
			m_app->dispToConsole("Failed to detect the peaks", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			return;
		}

		if (peaks.empty())
		{
			m_app->dispToConsole("No peak detected", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
			return;
		}
		m_app->dispToConsole(QString("%1 peak(s) detected").arg(peaks.size()));

		//simulate the gaussian filter (for debug)
		std::vector<double> smoothValues;
		smoothValues = values;
		if (specifParams.smoothWaveformSigma > 0)
		{
			FWFPeakDetectorInterface::SmoothSignal(smoothValues, specifParams.smoothWaveformSigma);
		}

		//create a new series of values to represent the detected peaks
		std::vector<double> peakValues, peakPos, peakSigma;
		peakValues.resize(values.size(), 0);
		peakPos.resize(values.size(), 0);
		peakSigma.resize(values.size(), 0);
		for (FWFPeakDetectorInterface::Peak peak : peaks)
		{
			size_t index = std::min(static_cast<size_t>(std::max(0.0, peak.x)), values.size());
			peakValues[index] = values[index];
			peakPos[index] = peak.x;
			peakSigma[index] = peak.sigma;
		}

		//and generate the synthetic waveform as well
		std::vector<double> syntheticValues;
		FWFPeakDetectorNLSGauss::GenerateSyntheticWaveform(peaks, values.size(), syntheticValues);

		//write the data as separate CSV files
		//ccWaveform::ToASCII(QString("waveform_%1_origin.csv").arg(pointIndex), originalValues, wd.samplingRate_ps);
		//ccWaveform::ToASCII(QString("waveform_%1_smooth.csv").arg(pointIndex), values, wd.samplingRate_ps);
		//ccWaveform::ToASCII(QString("waveform_%1_detected[%2].csv").arg(pointIndex).arg(peaks.size()), peakValues, wd.samplingRate_ps);

		//write the data to a CSV file
		{
			QString filename = QString("waveform_%1_processed.csv").arg(pointIndex);
			QFile file(filename);
			if (!file.open(QFile::Text | QFile::WriteOnly))
			{
				m_app->dispToConsole(QString("Failed to open file '%1' for writing").arg(filename), ccMainAppInterface::WRN_CONSOLE_MESSAGE);
				return;
			}

			QTextStream stream(&file);
			stream.setRealNumberPrecision(6);
			stream.setRealNumberNotation(QTextStream::FixedNotation);
			stream << "time(ps);original;smoothed;synthetic;peaks;peaks_pos;peaks_sigma" << endl;

			for (uint32_t i = 0; i < values.size(); ++i)
			{
				stream << i * w.descriptor().samplingRate_ps << ";" << values[i] << ";" << smoothValues[i] << ";" << syntheticValues[i] << ";" << peakValues[i] << "; " << peakPos[i] << "; " << peakSigma[i] << endl;
			}

			file.close();
			m_app->dispToConsole(QString("File '%1' has been saved successfully").arg(filename));
		}
	}
	catch (const std::bad_alloc&)
	{
		m_app->dispToConsole("Not enough memory to extract or save the peaks", ccMainAppInterface::WRN_CONSOLE_MESSAGE);
	}
}

QIcon qFWF::getIcon() const
{
	return QIcon(QString::fromUtf8(":/CC/plugin/qFWF/wave.png"));
}
