#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QtConcurrent/QtConcurrent>
#include <QImage>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QAudioSink>
#include <atomic> 
#include "Processing.NDI.Lib.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT


protected:
    void closeEvent(QCloseEvent* event) override;
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private:
    Ui::MainWindow *ui;

    NDIlib_find_create_t m_find_create;
    NDIlib_find_instance_t m_findInstance;
    NDIlib_recv_instance_t m_pNDIlibRecv;
    NDIlib_framesync_instance_t m_pNdiFrameSync;

    QList<QAudioDevice> m_detectedAudioDevices;
    QAudioDevice m_defaultAudioDevice{ QMediaDevices::defaultAudioOutput() }; // This can take significant time to call; do it here rather than in an audio loop
    QAudioDevice m_selectedAudioDevice{ m_defaultAudioDevice }; // Make it easy for users who don't want to pick through audio devices
    bool m_audioIdentified{ false };
    QAudioSink* m_pAudioSink{ nullptr }; // REPLACE this with memory safe option. Note QObjects are sometimes awkward about being created/deleted across threads.
    QIODevice* m_pAudioSinkIoDevice{ nullptr }; // Pointer to the QT audio sink's IO device - this is where audio data is written to for output
    QAudioFormat m_currentAudioFormat;

    void captureAudioFrame(NDIlib_framesync_instance_t const& pNdiFrameSync, uint const microsecondsSinceLastCapture);
    void processOutputAudioFrame(NDIlib_audio_frame_v2_t const& audio_frame, int const numSamplesToFetchPerChannel);
    void identifyAudioParameters(NDIlib_audio_frame_v2_t const& audio_frame);
    std::vector<float>      m_vecInterleavedData;   // For NDI utility function to write interleaved audio data into
    QByteArray              m_audioOutputsBuffer;   // Holding buffer for audio data before passing to the Qt audio sink's IO device

    void log(QString const& logline, bool doLog = true);

    QFutureWatcher<QStringList>* findSourcesWatcher{ new QFutureWatcher<QStringList>(this) };
    void findSourcesFinished();
    void launchFindNDISources();
    QStringList findNDISources();

    QFutureWatcher<QImage>* captureVideoFrameWatcher{ new QFutureWatcher<QImage>(this) };
    QImage captureVideoFrame(QString sourceName);
    void launchCaptureVideoFrame();
    void captureVideoFrameFinished();

    QFutureWatcher<bool>* playVideoWatcher{ new QFutureWatcher<bool>(this) };
    std::atomic<bool> m_stopPlayingOut{ true };
    bool playVideo(QString sourceName);
    void redetectSoundDevices();
    void selectedSoundDeviceChanged(int const index);
    void launchPlayVideo();
    void playVideoFinished();
    void captureAndProcessForDisplayVideoFrame(NDIlib_framesync_instance_t const& pNdiFrameSync);
signals:
    void logOnWidgetThread(QString);
    void updateVideoPlaybackImage(QPixmap);
};
#endif // MAINWINDOW_H
