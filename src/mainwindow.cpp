
#include <QPushButton>
#include <QDateTime>
#include <QMessageBox>
#include <chrono>

#include "NDIDeleters.h"

#include "mainwindow.h"
#include "./ui_mainwindow.h"

// Convert an NDI video frame to QImage
QImage NDIFrameToQImage(const NDIlib_video_frame_v2_t& videoFrame)
{
	if (videoFrame.p_data == nullptr)
	{
		return {};
	}

	// Assuming BGRA format (most common for NDI)
	QImage image(videoFrame.p_data,
		videoFrame.xres,
		videoFrame.yres,
		QImage::Format_ARGB32);

	return image.rgbSwapped();  // Convert BGRA -> RGBA for Qt
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
	log("Starting...");

	connect(this, &MainWindow::logOnWidgetThread,
		    ui->debugOutput, &QTextEdit::append);

	connect(this, &MainWindow::updateVideoPlaybackImage,
		    ui->labelPlayback, &QLabel::setPixmap);

	connect(ui->buttonScanForStreams, &QPushButton::clicked, this, &MainWindow::launchFindNDISources);
	connect(ui->buttonCaptureVideoFrame, &QPushButton::clicked, this, &MainWindow::launchCaptureVideoFrame);
	connect(ui->buttonPlayVideo, &QPushButton::clicked, this, &MainWindow::launchPlayVideo);
	connect(ui->buttonStopVideo, &QPushButton::clicked, this, [&m_stopPlayingOut = m_stopPlayingOut]() {m_stopPlayingOut.store(true);});
	connect(ui->buttonRedetectSoundDevices, &QPushButton::clicked, this, &MainWindow::redetectSoundDevices);
	connect(ui->cbSoundDevices, &QComboBox::currentIndexChanged, this, &MainWindow::selectedSoundDeviceChanged);
	connect(findSourcesWatcher, &QFutureWatcher<QStringList>::finished, this, &MainWindow::findSourcesFinished);
	connect(captureVideoFrameWatcher, &QFutureWatcher<QImage>::finished, this, & MainWindow::captureVideoFrameFinished);
	connect(playVideoWatcher, &QFutureWatcher<bool>::finished, this, &MainWindow::playVideoFinished);

	log(QString("Default audio output device detected as: ID: %1. Description: %2.").arg(m_defaultAudioDevice.id()).arg(m_defaultAudioDevice.description()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::selectedSoundDeviceChanged(int const index)
{
	m_selectedAudioDevice = m_detectedAudioDevices.at(index);
	log(QString("Selected sound device updated: %1").arg(m_selectedAudioDevice.description()));
}

void MainWindow::launchFindNDISources()
{
	ui->listWidgetStreamsFound->clear();
	// Launch function to find NDI sources on another thread, so as not to block the
	//  GUI thread.

	QFuture<QStringList> future = QtConcurrent::run(&MainWindow::findNDISources, this);
	findSourcesWatcher->setFuture(future);
}

void MainWindow::findSourcesFinished()
{
	auto result = findSourcesWatcher->future().result();
	for (auto& val : result)
	{
		ui->listWidgetStreamsFound->addItem(val);
	}
}

QStringList MainWindow::findNDISources()
{
	log("Scan for NDI sources started");
	
	NDIlib_find_instance_t pFind = NDIlib_find_create_v2();
	deleteGuard findInstanceGuard(NDIlib_find_destroy, pFind);

	if (!pFind)
	{
		log("NDIlib_find_create_v2 failed");
		return {};
	}

	if (!NDIlib_find_wait_for_sources(pFind, 5000 /* milliseconds */)) 
	{
		log(QString("No change to the sources found."));
	}

	// Get the updated list of sources
	uint32_t no_sources = 0;
	const NDIlib_source_t* p_sources = NDIlib_find_get_current_sources(pFind, &no_sources);
	// Display all the sources.
	log(QString("Network sources (%1 found).").arg(no_sources));
	
	QStringList foundSources;
	for (uint32_t i = 0; i < no_sources; i++)
	{
		foundSources.append(QString(p_sources[i].p_ndi_name));
	}

	return foundSources;
}

void MainWindow::log(QString const& logline, bool doLog)
{
	if (doLog)
	{
		emit logOnWidgetThread(QDateTime::currentDateTime().toString(Qt::ISODateWithMs) + " - " + logline);
	}
}

void MainWindow::launchCaptureVideoFrame()
{
	if (!ui->listWidgetStreamsFound->currentItem())
	{
		log("Select source before capturing video frame");
		QMessageBox::warning(this, "No source selected", "Select a source before capturing video frame");
		return;
	}
	ui->buttonCaptureVideoFrame->setEnabled(false);
	QString const selectedSource{ ui->listWidgetStreamsFound->currentItem()->text() };
	log(QString("Video frame capture from source: %1").arg(selectedSource));
	captureVideoFrameWatcher->setFuture(QtConcurrent::run(&MainWindow::captureVideoFrame, this, selectedSource));
}

void MainWindow::closeEvent(QCloseEvent* event) // Called when user closes application
{
	ui->buttonStopVideo->click(); // Elegantly (we hope!) halt the infinite loop of playing out, rather than violating QT rules about threads and objects
	// Don't accept or ignore the event; it will propogate upwards on its own
}

QImage MainWindow::captureVideoFrame(QString sourceName)
{
	// This function sets up and tears down the NDI receiving objects every time it is called.
	// This seems to be fairly expensive (as in, takes a noticable amount of time) but for the 
	// simple purposes of just grabbing one frame to display, that's fine.

	const auto streamStdString = sourceName.toStdString();
	NDIlib_source_t source{ streamStdString.c_str() };
	NDIlib_recv_create_v3_t const recvSettings{ source, NDIlib_recv_color_format_RGBX_RGBA, NDIlib_recv_bandwidth_highest, false };

	NDIlib_recv_instance_t const pNDIlibRecv = NDIlib_recv_create_v3(&recvSettings);
	deleteGuard NdiRecvGuard(NDIlib_recv_destroy, pNDIlibRecv);
	if (!pNDIlibRecv)
	{
		return {};
	}

	NDIlib_video_frame_v2_t video_frame;
	QImage imToReturn;

	int tries = 0;
	while (tries++ < 10)
	{
		// Depending on the source, it seems that even though we're only asking for a video frame, we can get other kinds
		//  of responses to the capture request. Hence multiple attempts.
		NDIlib_frame_type_e frame_type = NDIlib_recv_capture_v2(pNDIlibRecv, &video_frame, nullptr, nullptr, 5000);

		if (frame_type == NDIlib_frame_type_video) 
		{
			// Convert NDI frame to QImage
			imToReturn = NDIFrameToQImage(video_frame);

			// Free the video frame
			NDIlib_recv_free_video_v2(pNDIlibRecv, &video_frame);
			break;
		}
	}

	return imToReturn;
}

void MainWindow::captureVideoFrameFinished()
{
	auto result = captureVideoFrameWatcher->future().result();
	log("Video frame capture complete");
	QSize const labelSize{ ui->labelVideoFrame->size() };
	ui->labelVideoFrame->setPixmap(QPixmap::fromImage(result.scaled(labelSize, Qt::KeepAspectRatio)));
	log(QString("Captured frame size: %1x%2. Scaled to fit: %3x%4").arg(result.width()).arg(result.height()).arg(labelSize.width()).arg(labelSize.height()));
	ui->buttonCaptureVideoFrame->setEnabled(true);
}



void MainWindow::captureAudioFrame(NDIlib_framesync_instance_t const& pNdiFrameSync, uint const microsecondsSinceLastCapture)
{
	{
		NDIlib_audio_frame_v2_t audio_frame;

		if (!m_audioIdentified) 
		{
			NDIlib_framesync_capture_audio(pNdiFrameSync, &audio_frame, 0, 0, 0);
			if (m_pAudioSink) // Could have leftover audio sink from earlier captures
			{
				delete m_pAudioSink;
				m_pAudioSink = nullptr;
			}
			identifyAudioParameters(audio_frame);
		}
		else if (m_pAudioSink)
		{
			if (m_pAudioSink->state() == QAudio::StoppedState)
			{
				log("Starting audiosink");
				m_pAudioSinkIoDevice = m_pAudioSink->start(); // The QAudioSink, when started, provides a pointer to a QIODevice to which to write the audio data
				log("Audiosink started");
			}
			else
			{
				// Audio sink was previously started. Let's just start putting audio into it
				int const numSamplesToFetchPerChannel = m_currentAudioFormat.sampleRate() * (microsecondsSinceLastCapture / 1000000.0);
				NDIlib_framesync_capture_audio(
					pNdiFrameSync, // The frame sync instance. NDILib object
					&audio_frame, // The destination audio buffer. NDILib object 
					m_currentAudioFormat.sampleRate(),
					m_currentAudioFormat.channelCount(), // 2 channels to draw
					numSamplesToFetchPerChannel);

				processOutputAudioFrame(audio_frame, numSamplesToFetchPerChannel);
			}
		}
		NDIlib_framesync_free_audio(pNdiFrameSync, &audio_frame);
	}

}

void MainWindow::processOutputAudioFrame(NDIlib_audio_frame_v2_t const& audio_frame, int const numSamplesToFetchPerChannel)
{
	// The NDILib structure stores audio data in a planar format, meaning each channel's samples are grouped together. 
	//  However, QAudioSink expects interleaved data. Fortunately, an NDILib utility function can handle the conversion, 
	//  but we still need to ensure the recipient structure is properly populated with data.
	NDIlib_audio_frame_interleaved_32f_t ndiInterleavedAudioStore;
	ndiInterleavedAudioStore.sample_rate = m_currentAudioFormat.sampleRate();
	ndiInterleavedAudioStore.no_samples = numSamplesToFetchPerChannel;
	ndiInterleavedAudioStore.no_channels = m_currentAudioFormat.channelCount();

	int const totalNumberOfSamplesAcrossAllChannels = numSamplesToFetchPerChannel * m_currentAudioFormat.channelCount();

	// We must also provide the memory for the rearranged audio data to occupy.
	if (m_vecInterleavedData.size() < totalNumberOfSamplesAcrossAllChannels)
	{
		m_vecInterleavedData.resize(totalNumberOfSamplesAcrossAllChannels);
	}
	ndiInterleavedAudioStore.p_data = &(m_vecInterleavedData[0]);

	// This is the utility function that turns planar audio data into interleaved
	NDIlib_util_audio_to_interleaved_32f_v2(&audio_frame, &ndiInterleavedAudioStore);

	m_audioOutputsBuffer.clear();

	// The audio data will go to a QByteArray to be ingested by the QAudioSink's
	//  QIODevice. Accordingly the array of floats must be copied into that QByteArray.
	m_audioOutputsBuffer.append(reinterpret_cast<const char*>(ndiInterleavedAudioStore.p_data), sizeof(float) * totalNumberOfSamplesAcrossAllChannels);
	log(QString("Stored %1 bytes in buffer.").arg(m_audioOutputsBuffer.size()), ui->checkBoxVideoPlaybackDebugLogging->isChecked());

	if (m_pAudioSinkIoDevice)
	{
		qsizetype const bytesFree = m_pAudioSink->bytesFree();
		log(QString("Audio sink has %1 bytes free").arg(bytesFree), ui->checkBoxVideoPlaybackDebugLogging->isChecked());
		if (bytesFree < m_audioOutputsBuffer.size())
		{
			log("Audiosink buffer has too little space for current audio data. Some audio data will be dropped.");
		}
		qint64 const bytesWritten = m_pAudioSinkIoDevice->write(m_audioOutputsBuffer.data(), qMin(m_audioOutputsBuffer.size(), bytesFree));
		log(QString("Wrote %1 bytes to audiosink").arg(bytesWritten), ui->checkBoxVideoPlaybackDebugLogging->isChecked());
	}
	else
	{
		log("Could not obtain IO device for audiosink; cannot send audio data to output audio device.");
	}
}


void MainWindow::captureAndProcessForDisplayVideoFrame(NDIlib_framesync_instance_t const& pNdiFrameSync)
{
	NDIlib_video_frame_v2_t video_frame;

	NDIlib_framesync_capture_video(
		pNdiFrameSync,
		&video_frame, // Write data into here
		NDIlib_frame_format_type_progressive);

	if (video_frame.yres > 0) // Used as proxy for knowing an actual frame of video data was captured
	{
		log(QString("Captured video frame, size %1x%2").arg(video_frame.yres).arg(video_frame.xres), 
			ui->checkBoxVideoPlaybackDebugLogging->isChecked());
	}

	if (QImage imageToStore(video_frame.p_data, video_frame.xres, video_frame.yres, QImage::Format_RGBX8888);
		!imageToStore.isNull())
	{
		emit updateVideoPlaybackImage(QPixmap::fromImage(imageToStore.scaled(ui->labelPlayback->size(), Qt::KeepAspectRatio)));
	}

	NDIlib_framesync_free_video(pNdiFrameSync, &video_frame);
}

void MainWindow::redetectSoundDevices()
{
	if (m_stopPlayingOut.load() != true) // Used as proxy for "are we playing out?"
	{
		QMessageBox::warning(this, "Detect audio devices", "Cannot detect audio devices while playing out");
		return;
	}


	// This is being done on the MainWindow's thread. This should make it safe to directly call on the UI elements, 
	// BUT if this function takes a while, the GUI will freeze. If that's an issue, move the detection of the audio
	// output devices to a separate thread and when that thread is done, then update the UI.
	log(QString("Detecting output audio devices"));
	ui->cbSoundDevices->clear();

	m_detectedAudioDevices = QMediaDevices::audioOutputs();

	for (auto const& device : m_detectedAudioDevices)
	{
		log(QString("Detected audio output device: ID: %1. Description: %2.").arg(device.id()).arg(device.description()));
		ui->cbSoundDevices->addItem(device.description());
	}
}

void MainWindow::launchPlayVideo()
{
	if (!ui->listWidgetStreamsFound->currentItem())
	{
		log("Select source before capturing video frame");
		return;
	}
	QString const selectedSource{ ui->listWidgetStreamsFound->currentItem()->text() };
	ui->buttonPlayVideo->setEnabled(false);
	log(QString("Launching play video from source %1").arg(selectedSource));
	QFuture<bool> future = QtConcurrent::run(&MainWindow::playVideo, this, selectedSource);
	playVideoWatcher->setFuture(future);
}

bool MainWindow::playVideo(QString sourceName)
{
	const auto streamStdString = sourceName.toStdString();
	NDIlib_source_t source{ streamStdString.c_str() };
	NDIlib_recv_create_v3_t const recvSettings{ source, 
		                                        NDIlib_recv_color_format_RGBX_RGBA, 
		                                        ui->comboBoxVideoQuality->currentText() == "Full" ? NDIlib_recv_bandwidth_highest : NDIlib_recv_bandwidth_lowest,
		                                        false };

	NDIlib_recv_instance_t const pNDIlibRecv = NDIlib_recv_create_v3(&recvSettings);
	deleteGuard NdiRecvGuard(NDIlib_recv_destroy, pNDIlibRecv);
	if (!pNDIlibRecv)
	{
		log("NDIlib_recv_create_v3 failed");
		return false;
	}

	NDIlib_framesync_instance_t const pNdiFrameSync = NDIlib_framesync_create(pNDIlibRecv);
	deleteGuard frameSyncguard(NDIlib_recv_destroy, pNDIlibRecv);
	if (!pNdiFrameSync)
	{
		log("NDIlib_framesync_create failed");
		return false;
	}

	int m_NdiDataCapturesPerSecond = ui->spinBoxCapturesperSecond->value();
	uint const microsecondsBetweenCaptures = 1000000 / m_NdiDataCapturesPerSecond; // Nearest microsecond is good enough
	std::chrono::time_point<std::chrono::steady_clock> m_timeOflastSample{ std::chrono::steady_clock::now() }; //When to take a new sample

	m_audioIdentified = false;
	m_stopPlayingOut.store(false);
	while (m_stopPlayingOut.load() != true)       //  loop until someone external orders a stop
	{
		using namespace std::chrono;
		if (auto const microSecondsSinceLastCapture = static_cast<uint>(duration_cast<microseconds>(steady_clock::now() - m_timeOflastSample).count());
			microSecondsSinceLastCapture >= microsecondsBetweenCaptures)
		{
			m_timeOflastSample = steady_clock::now();
			captureAndProcessForDisplayVideoFrame(pNdiFrameSync);

			if (ui->checkBoxAudio->isChecked())
			{
				captureAudioFrame(pNdiFrameSync, microSecondsSinceLastCapture);
			}

			log(QString("Captured video, and audio if appropriate. Microseconds since previous capture = %1").arg(microSecondsSinceLastCapture),
				ui->checkBoxVideoPlaybackDebugLogging->isChecked());
		}
	}
	if (m_pAudioSink) 
	{
		delete m_pAudioSink;
		m_pAudioSink = nullptr;
	} // The QAudioSink is a QObject with QTimers, and should be destroyed on same thread it is created i.e. here
	return true;
}

void MainWindow::playVideoFinished()
{
	log("Play video complete");
	ui->buttonPlayVideo->setEnabled(true);
}

void MainWindow::identifyAudioParameters(NDIlib_audio_frame_v2_t const& audio_frame)
{
	if (audio_frame.no_channels != 0) // Used as a proxy for having real data - assume any real audio data must have at least one channel
	{
		log(QString("Audio data detected. Num channels = %1, sample rate = %2 Hz, metadata: %3")
			.arg(audio_frame.no_channels)
			.arg(audio_frame.sample_rate)
			.arg(audio_frame.p_metadata));

		m_currentAudioFormat = QAudioFormat();

		m_currentAudioFormat.setSampleRate(audio_frame.sample_rate);
		m_currentAudioFormat.setChannelCount(audio_frame.no_channels);
		m_currentAudioFormat.setSampleFormat(QAudioFormat::Float);

		if (!m_selectedAudioDevice.isFormatSupported(m_currentAudioFormat))
		{
			log(QString("Audio format not supported by device, cannot play audio. Sample rate: %1 , channel count: %2 , sample format: FLOAT")
				       .arg(audio_frame.sample_rate).arg(audio_frame.no_channels));

			m_currentAudioFormat = m_selectedAudioDevice.preferredFormat();
			log(QString("Will attempt preferred format: Sample rate: %1 , channel count: %2 , sample format: %3")
				.arg(m_currentAudioFormat.sampleRate()).arg(m_currentAudioFormat.channelCount()).arg(m_currentAudioFormat.sampleFormat()));

			if (m_pAudioSink)
			{
				delete m_pAudioSink;
				m_pAudioSink = nullptr;
			}
			m_pAudioSink = new QAudioSink(m_selectedAudioDevice, m_currentAudioFormat);
		}
		else
		{
			log("Audio format supported by device.", ui->checkBoxVideoPlaybackDebugLogging->isChecked());

			if (m_pAudioSink)
			{
				delete m_pAudioSink;
				m_pAudioSink = nullptr;
			}
			m_pAudioSink = new QAudioSink(m_selectedAudioDevice, m_currentAudioFormat);
		}
		m_audioIdentified = true;
	}
	else
	{
		log("No audio captured");
	}
}