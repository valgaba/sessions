#include "multimedia.h"
#include <QtWidgets>
#include "slider.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <functional>
#include <cstdio>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propidl.h>
#endif

struct VideoFrames {
    quint64 serial=0;
    QMutex mutex;
    QByteArray storage;
    unsigned width=0,height=0,pitch=0;
    uchar *pixels=nullptr;
    QSize maximumSize{1920,1080};
    std::atomic_bool queued{false};
    std::atomic<quint64> produced{0};
    std::function<void()> notify;
    static unsigned format(void **opaque,char *chroma,unsigned *width,unsigned *height,unsigned *pitches,unsigned *lines) {
        auto *frames=static_cast<VideoFrames *>(*opaque);QMutexLocker guard(&frames->mutex);
        QSize size{int(*width),int(*height)};
        if(size.width()>frames->maximumSize.width() || size.height()>frames->maximumSize.height())size.scale(frames->maximumSize,Qt::KeepAspectRatio);
        *width=unsigned(qMax(1,size.width()));*height=unsigned(qMax(1,size.height()));
        std::memcpy(chroma,"RV32",4);frames->width=*width;frames->height=*height;frames->pitch=(*width*4+31)&~31u;
        // VLC's optimized converters can process padded scanlines. Allocate
        // both pitch and line count in multiples of 32, as its callback API
        // recommends, while keeping the visible image dimensions unchanged.
        const unsigned allocatedLines=(*height+31)&~31u;
        frames->storage.resize(qsizetype(frames->pitch)*allocatedLines+31);
        frames->pixels=reinterpret_cast<uchar *>((reinterpret_cast<quintptr>(frames->storage.data())+31)&~quintptr(31));
        *pitches=frames->pitch;*lines=allocatedLines;return 1;
    }
    static void *lock(void *opaque,void **planes) {
        auto *frames=static_cast<VideoFrames *>(opaque);frames->mutex.lock();*planes=frames->pixels;return nullptr;
    }
    static void unlock(void *opaque,void *,void *const *) {static_cast<VideoFrames *>(opaque)->mutex.unlock();}
    static void display(void *opaque,void *) {
        auto *frames=static_cast<VideoFrames *>(opaque);
        ++frames->produced;
        if(!frames->queued.exchange(true))frames->notify();
    }
};

// LibVLC 3 public C ABI. Resolve dynamically so the rest of Sessions works
// without VLC, and never load libraries from the current working directory.
namespace {
struct VlcAudioDevice {
    VlcAudioDevice *next;
    char *id;
    char *description;
};
struct VlcApi {
    QLibrary core, library;
    void *(*create)(int,const char *const *) = nullptr;
    void (*release)(void *) = nullptr;
    void *(*newPlayer)(void *) = nullptr;
    void (*releasePlayer)(void *) = nullptr;
    void *(*newMedia)(void *,const char *) = nullptr;
    void (*releaseMedia)(void *) = nullptr;
    void (*mediaOption)(void *,const char *) = nullptr;
    int (*parseMedia)(void *,int,int) = nullptr;
    int (*parsedStatus)(void *) = nullptr;
    void (*stopParsing)(void *) = nullptr;
    qint64 (*mediaDuration)(void *) = nullptr;
    void (*setMedia)(void *,void *) = nullptr;
    int (*play)(void *) = nullptr;
    void (*pause)(void *,int) = nullptr;
    void (*stop)(void *) = nullptr;
    qint64 (*getTime)(void *) = nullptr;
    qint64 (*getLength)(void *) = nullptr;
    void (*setTime)(void *,qint64) = nullptr;
    int (*getState)(void *) = nullptr;
    int (*seekable)(void *) = nullptr;
    unsigned (*videoCount)(void *) = nullptr;
    void (*setCallbacks)(void *,void *(*)(void *,void **),void (*)(void *,void *,void *const *),void (*)(void *,void *),void *) = nullptr;
    void (*setFormat)(void *,unsigned (*)(void **,char *,unsigned *,unsigned *,unsigned *,unsigned *),void (*)(void *)) = nullptr;
    void (*mute)(void *,int) = nullptr;
    int (*getMute)(void *) = nullptr;
    void (*audioDeviceSet)(void *,const char *,const char *) = nullptr;
    VlcAudioDevice *(*audioDeviceEnum)(void *) = nullptr;
    void (*audioDeviceListRelease)(VlcAudioDevice *) = nullptr;
    char *(*audioDeviceGet)(void *) = nullptr;
    void (*freeMemory)(void *) = nullptr;
    int (*audioSetVolume)(void *,int) = nullptr;
    int (*audioGetVolume)(void *) = nullptr;
    bool ready = false;
    bool deviceSelectionAvailable() const { return ready && audioDeviceSet && audioDeviceEnum && audioDeviceListRelease; }
    bool volumeAvailable() const { return ready && audioSetVolume && audioGetVolume; }
    template<class T> bool resolve(T &function,const char *symbol) {
        function = reinterpret_cast<T>(library.resolve(symbol)); return function != nullptr;
    }
    VlcApi() {
        const QStringList folders{QCoreApplication::applicationDirPath()+"/vlc",qEnvironmentVariable("ProgramW6432","C:/Program Files")+"/VideoLAN/VLC"};
        for (const auto &folder:folders) {
            if (!QFileInfo::exists(folder+"/libvlc.dll")) continue;
            core.setFileName(folder+"/libvlccore.dll"); if(!core.load())continue;
            library.setFileName(folder+"/libvlc.dll"); if(!library.load())continue;
            using Version = const char *(*)();
            auto version=reinterpret_cast<Version>(library.resolve("libvlc_get_version"));
            if(!version || !QByteArray(version()).startsWith("3."))return;
            ready = resolve(create,"libvlc_new") && resolve(release,"libvlc_release")
                && resolve(newPlayer,"libvlc_media_player_new") && resolve(releasePlayer,"libvlc_media_player_release")
                && resolve(newMedia,"libvlc_media_new_path") && resolve(releaseMedia,"libvlc_media_release")
                && resolve(mediaOption,"libvlc_media_add_option")
                && resolve(setMedia,"libvlc_media_player_set_media") && resolve(play,"libvlc_media_player_play")
                && resolve(pause,"libvlc_media_player_set_pause") && resolve(stop,"libvlc_media_player_stop")
                && resolve(getTime,"libvlc_media_player_get_time") && resolve(getLength,"libvlc_media_player_get_length")
                && resolve(setTime,"libvlc_media_player_set_time") && resolve(getState,"libvlc_media_player_get_state")
                && resolve(seekable,"libvlc_media_player_is_seekable") && resolve(videoCount,"libvlc_media_player_has_vout")
                && resolve(setCallbacks,"libvlc_video_set_callbacks") && resolve(setFormat,"libvlc_video_set_format_callbacks")
                && resolve(mute,"libvlc_audio_set_mute") && resolve(getMute,"libvlc_audio_get_mute");
            // Optional: audio output selection. Older LibVLC builds still play
            // with the system default when these symbols are missing.
            resolve(audioDeviceSet,"libvlc_audio_output_device_set");
            resolve(audioDeviceEnum,"libvlc_audio_output_device_enum");
            resolve(audioDeviceListRelease,"libvlc_audio_output_device_list_release");
            resolve(audioDeviceGet,"libvlc_audio_output_device_get");
            resolve(freeMemory,"libvlc_free");
            resolve(audioSetVolume,"libvlc_audio_set_volume");
            resolve(audioGetVolume,"libvlc_audio_get_volume");
            resolve(parseMedia,"libvlc_media_parse_with_options");
            resolve(parsedStatus,"libvlc_media_get_parsed_status");
            resolve(stopParsing,"libvlc_media_parse_stop");
            resolve(mediaDuration,"libvlc_media_get_duration");
            return;
        }
    }
};
VlcApi &vlc() { static VlcApi api; return api; }
QString clockTime(qint64 milliseconds) {
    const qint64 seconds=qMax(qint64(0),milliseconds)/1000;
    return QString("%1:%2:%3").arg(seconds/3600,2,10,QLatin1Char('0')).arg(seconds/60%60,2,10,QLatin1Char('0')).arg(seconds%60,2,10,QLatin1Char('0'));
}
}
struct MediaPlayback {
    void *instance=nullptr;
    void *player=nullptr;
    MediaPlayback() {
        auto &api=vlc(); if(!api.ready)return;
        const char *options[]{"--quiet","--no-video-title-show","--no-metadata-network-access","--ignore-config","--no-loop","--no-repeat"};
        if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))options[0]="--verbose=2";
        instance=api.create(6,options); if(instance)player=api.newPlayer(instance);
    }
    ~MediaPlayback() {
        auto &api=vlc(); if(player){api.stop(player);api.releasePlayer(player);} if(instance)api.release(instance);
    }
};
// A separate muted decoder prepares a thumbnail without launching or cueing media.
struct ThumbnailProbe : QObject {
    std::unique_ptr<QThread> preparation;
    QImage preparedImage;
    qint64 preparedDuration=0;
    std::unique_ptr<MediaPlayback> player;
    std::unique_ptr<VideoFrames> frames;
    QTimer timeout;
    std::function<void(const QImage &,qint64)> completed;
    explicit ThumbnailProbe(std::function<void(const QImage &,qint64)> callback):completed(std::move(callback)) {
        timeout.setSingleShot(true);connect(&timeout,&QTimer::timeout,this,[this]{finish({});});
    }
    ~ThumbnailProbe() override {if(preparation){preparation->requestInterruption();preparation->wait();}player.reset();frames.reset();}
    void finish(const QImage &image) {
        if(player && player->player)preparedDuration=qMax(preparedDuration,vlc().getLength(player->player));
        timeout.stop();player.reset();frames.reset();
        auto callback=std::move(completed);completed={};if(callback)callback(image,preparedDuration);
    }
    void start(const QString &path,bool audioOnly=false,bool imageOnly=false) {
        // Loading LibVLC's plugins or reading a large image must not block
        // the GUI, otherwise its loading indicator cannot animate.
        preparation.reset(QThread::create([this,path,imageOnly]{
            if(imageOnly) {
                QImageReader reader(path);reader.setAutoTransform(true);
                const auto size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(320,180,Qt::KeepAspectRatio));
                preparedImage=reader.read();
            } else {
                player=std::make_unique<MediaPlayback>();
                auto &api=vlc();
                if(!player->player || !api.parseMedia || !api.parsedStatus || !api.stopParsing || !api.mediaDuration)return;
                auto *media=api.newMedia(player->instance,QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8().constData());
                if(!media)return;
                // Read local metadata without playing audio or sending frames.
                if(api.parseMedia(media,0,5000)==0) {
                    QElapsedTimer deadline;deadline.start();
                    while(api.parsedStatus(media)==0 && deadline.elapsed()<5500 && !QThread::currentThread()->isInterruptionRequested())QThread::msleep(20);
                    preparedDuration=qMax(qint64(0),api.mediaDuration(media));
                    api.stopParsing(media);
                }
                api.releaseMedia(media);
            }
        }));
        connect(preparation.get(),&QThread::finished,this,[this,path,audioOnly,imageOnly]{
            if(imageOnly){finish(preparedImage);return;}
            if(!player || !player->player || audioOnly){finish({});return;}
            startVideo(path);
        });
        preparation->start();
    }
    void startVideo(const QString &path) {
        auto &api=vlc();auto *media=api.newMedia(player->instance,QDir::toNativeSeparators(path).toUtf8().constData());
        if(!media){finish({});return;}
        api.mediaOption(media,":no-audio");api.mediaOption(media,":no-spu");
        // A thumbnail decoder is stopped immediately after its first frame.
        // Avoid DXV's threaded decoder teardown race in VLC 3/FFmpeg.
        api.mediaOption(media,":avcodec-threads=1");
        if(QStringList{"mp4","mov","m4v"}.contains(QFileInfo(path).suffix().toLower()))api.mediaOption(media,":demux=avformat");
        api.setMedia(player->player,media);api.releaseMedia(media);api.mute(player->player,1);
        frames=std::make_unique<VideoFrames>();frames->maximumSize=QSize(320,180);
        frames->notify=[this]{QMetaObject::invokeMethod(this,[this]{
            if(!frames || !player)return;
            QImage image;{QMutexLocker guard(&frames->mutex);if(frames->pixels)image=QImage(frames->pixels,int(frames->width),int(frames->height),int(frames->pitch),QImage::Format_RGB32).copy();}
            finish(image);
        },Qt::QueuedConnection);};
        api.setCallbacks(player->player,VideoFrames::lock,VideoFrames::unlock,VideoFrames::display,frames.get());api.setFormat(player->player,VideoFrames::format,nullptr);
        timeout.start(5000);if(api.play(player->player)<0)finish({});
    }
};
Multimedia::Multimedia(QWidget *parent):QWidget(parent) {
    setObjectName("Multimedia"); setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(8,6,8,6); layout->setSpacing(6);
    auto *header=new QHBoxLayout;
    title=new QLabel(mediaName); title->setObjectName("MultimediaTitle");title->setTextFormat(Qt::PlainText);
    title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);header->addWidget(title,1);
    status=new QLabel("Sin archivo",this);status->setObjectName("MultimediaStatus");status->hide();
    time=new QLabel;time->setObjectName("MultimediaTime");time->setAlignment(Qt::AlignRight|Qt::AlignVCenter);updateTime(0,0);
    playButton=new QPushButton;playButton->setObjectName("multimediaPlay");playButton->setProperty("thumbnail",true);
    playButton->setIconSize(QSize(36,20));playButton->setToolTip("Play · Reproducir");playButton->setAccessibleName("Play");updateThumbnail({});
    pauseButton=new QPushButton(QIcon(":/icons/Pause.svg"),"Pause");pauseButton->setObjectName("multimediaPause");pauseButton->setToolTip("Pause");pauseButton->setAccessibleName("Pause");
    stopButton=new QPushButton(QIcon(":/icons/Stop.svg"),"Stop");stopButton->setObjectName("multimediaStop");stopButton->setToolTip("Stop");stopButton->setAccessibleName("Stop");
    loopButton=new QPushButton;loopButton->setObjectName("multimediaLoop");
    loopButton->setIcon(QIcon(":/icons/Loop.svg"));loopButton->setIconSize(QSize(16,16));
    loopButton->setFixedSize(26,26);loopButton->setCheckable(true);loopButton->setEnabled(false);
    loopButton->setToolTip("Loop · Repetir al terminar");loopButton->setAccessibleName("Loop");
    cueButton=new QPushButton(QIcon(":/icons/PlayCue.svg"),"Cue");cueButton->setObjectName("multimediaCue");cueButton->setEnabled(false);cueButton->setToolTip("Cue · Preescucha / composición");cueButton->setAccessibleName("Cue");
    purgeButton=new QPushButton(QIcon(":/icons/Purge.svg"),QString());purgeButton->setObjectName("multimediaPurge");purgeButton->setCheckable(true);purgeButton->setToolTip("Purge · Eliminar la acción al terminar");purgeButton->setAccessibleName("Purge");
    nextButton=new QPushButton(QIcon(":/icons/Next.svg"),QString());nextButton->setObjectName("multimediaNext");nextButton->setCheckable(true);nextButton->setToolTip("Next · Incluir en el avance automático hacia abajo, continuando desde arriba al llegar al final");nextButton->setAccessibleName("Next");
    mixButton=new QPushButton("Mix");mixButton->setObjectName("multimediaMix");mixButton->setCheckable(true);mixButton->setToolTip("Mix · Permitir mezclar este contenido al pulsar Play");
    header->setSpacing(3);
    for(auto *button:{mixButton,nextButton,purgeButton,loopButton}) {
        button->setFixedSize(button==mixButton?34:24,26);
        button->setProperty("compact",true);button->setIconSize(QSize(16,16));
    }
    for(auto *button:{playButton,cueButton,pauseButton,stopButton})button->setMinimumSize(64,28);
    for(auto *button:{nextButton,purgeButton,loopButton,mixButton})header->addWidget(button);
    layout->addLayout(header);
    auto *controls=new QHBoxLayout;controls->setSpacing(3);
    for(auto *button:{playButton,cueButton,pauseButton,stopButton})controls->addWidget(button);
    controls->addStretch();layout->addLayout(controls);
    loadingBar=new QProgressBar;loadingBar->setObjectName("multimediaLoading");
    loadingBar->setRange(0,0);loadingBar->setTextVisible(false);loadingBar->setFixedHeight(8);
    loadingBar->setToolTip("Cargando archivo…");loadingBar->setAccessibleName("Cargando multimedia");loadingBar->hide();layout->addWidget(loadingBar);
    slider=new Slider;slider->setObjectName("multimediaSlider");slider->setFixedHeight(26);slider->setEnabled(false);
    slider->setRange(0,100000);slider->setSingleStep(100);slider->setPageStep(1000);
    auto *timeline=new QHBoxLayout;timeline->setSpacing(8);timeline->addWidget(slider,1);timeline->addWidget(time);layout->addLayout(timeline);
    playButton->setEnabled(false);pauseButton->setEnabled(false);stopButton->setEnabled(false);
    connect(playButton,&QPushButton::clicked,this,&Multimedia::play);
    connect(cueButton,&QPushButton::clicked,this,&Multimedia::cue);
    connect(pauseButton,&QPushButton::clicked,this,&Multimedia::pause);
    connect(stopButton,&QPushButton::clicked,this,&Multimedia::stop);
    connect(loopButton,&QPushButton::toggled,this,[this](bool enabled){looping=enabled;emit configurationChanged();});
    connect(purgeButton,&QPushButton::toggled,this,&Multimedia::configurationChanged);
    connect(nextButton,&QPushButton::toggled,this,&Multimedia::configurationChanged);
    connect(mixButton,&QPushButton::toggled,this,&Multimedia::configurationChanged);
    connect(slider,&QSlider::sliderMoved,this,[this](int value){updateTime(duration()*value/slider->maximum(),duration());});
    connect(slider,&QSlider::sliderReleased,this,[this]{seek(duration()*slider->value()/slider->maximum());});
    connect(slider,&QSlider::actionTriggered,this,[this](int){if(!slider->isSliderDown())QTimer::singleShot(0,this,[this]{seek(duration()*slider->value()/slider->maximum());});});
    poll.setInterval(100);connect(&poll,&QTimer::timeout,this,&Multimedia::updatePlayback);
    timelineTimer.setInterval(16);timelineTimer.setTimerType(Qt::PreciseTimer);
    connect(&timelineTimer,&QTimer::timeout,this,&Multimedia::updateTimeline);
    setToolTip("Botón derecho: archivo y propiedades de Multimedia");
}
Multimedia::~Multimedia() {
    thumbnailProbe.reset();poll.stop();timelineTimer.stop();
    if(loopPreparationThread)loopPreparationThread->wait();
    loopPlayback.reset();loopFrames.reset();playback.reset();
}
void Multimedia::setTransportActive(QPushButton *active) {
    for(auto *button:{playButton,cueButton,pauseButton}) {
        const bool enabled=button==active;
        if(button->property("transportActive").toBool()==enabled)continue;
        button->setProperty("transportActive",enabled);
        button->style()->unpolish(button);button->style()->polish(button);button->update();
    }
}
void Multimedia::setLoading(bool loading) {
    loadingBar->setVisible(loading);
    loadingBar->setAccessibleDescription(loading?"Preparando el archivo":"Archivo preparado");
    updateGeometry();
}
void Multimedia::updateThumbnail(const QImage &image) {
    thumbnailImage=image;
    QPixmap preview(36,20);preview.fill(QColor("#181e2c"));QPainter painter(&preview);
    if(!image.isNull()) {
        const auto fitted=image.scaled(preview.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);
        painter.drawImage(QPoint((preview.width()-fitted.width())/2,(preview.height()-fitted.height())/2),fitted);
        painter.fillRect(QRect(22,6,14,14),QColor(24,30,44,200));
        QIcon(":/icons/Play.svg").paint(&painter,QRect(23,7,12,12));
    } else QIcon(":/icons/Play.svg").paint(&painter,QRect(10,2,16,16));
    painter.end();playButton->setIcon(QIcon(preview));
}
bool Multimedia::backendAvailable(){return vlc().ready;}
void Multimedia::warmUpBackend(const std::function<void(int,const QString &)> &progress) {
    const auto report=[&](int phase,const QString &text){if(progress)progress(phase,text);};
    report(0,"Cargando LibVLC…");
    auto &api=vlc();
    if(!api.ready){report(5,"Motor multimedia no disponible");return;}
    report(1,"Preparando plugins…");
    // Keep an instance alive so VLC retains its plugin registry after startup.
    static MediaPlayback warmup;
    if(!warmup.player){report(5,warmup.instance?"Motor multimedia preparado":"No se pudo preparar multimedia");return;}
    report(2,"Preparando decodificador…");
    QFile clip(":/media/warmup.mp4");
    QTemporaryFile file(QDir::tempPath()+"/sessions-warmup-XXXXXX.mp4");
    if(!clip.open(QIODevice::ReadOnly) || !file.open()){report(5,"Motor preparado; vídeo de precarga no disponible");return;}
    const QByteArray bytes=clip.readAll();
    if(file.write(bytes)!=bytes.size()){report(5,"Motor preparado; vídeo de precarga no disponible");return;}
    const QString filename=file.fileName();file.close();
    auto *media=api.newMedia(warmup.instance,QDir::toNativeSeparators(filename).toUtf8().constData());
    if(!media){report(5,"Motor preparado; vídeo de precarga no disponible");return;}
    api.mediaOption(media,":no-audio");api.mediaOption(media,":no-spu");
    api.mediaOption(media,":demux=avformat");api.mediaOption(media,":avcodec-threads=1");
    api.setMedia(warmup.player,media);api.releaseMedia(media);api.mute(warmup.player,1);
    VideoFrames frames;frames.maximumSize=QSize(32,32);frames.notify=[]{};
    api.setCallbacks(warmup.player,VideoFrames::lock,VideoFrames::unlock,VideoFrames::display,&frames);
    api.setFormat(warmup.player,VideoFrames::format,nullptr);
    report(3,"Preparando vídeo…");
    if(api.play(warmup.player)==0) {
        QElapsedTimer deadline;deadline.start();
        while(!frames.queued && deadline.elapsed()<3000) {
            const int state=api.getState(warmup.player);
            if(state==6 || state==7)break;
            QThread::msleep(20);
        }
    }
    report(4,"Finalizando precarga…");
    // Release callbacks before their local frame buffer goes out of scope.
    api.stop(warmup.player);api.releasePlayer(warmup.player);warmup.player=nullptr;
    report(5,frames.queued?"Multimedia preparado":"Inicialización multimedia finalizada");
}
QList<AudioOutputDevice> Multimedia::audioDevices() {
    QList<AudioOutputDevice> devices;
#ifdef Q_OS_WIN
    // Enumerate with WASAPI so opening the application does not load LibVLC
    // just to fill the device combos. VLC's mmdevice backend uses the same
    // endpoint identifiers.
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator *deviceEnumerator=nullptr;
    if(SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&deviceEnumerator)))) {
        devices.append({QString(),QStringLiteral("Predeterminado")});
        IMMDeviceCollection *collection=nullptr;
        if(SUCCEEDED(deviceEnumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,&collection)) && collection) {
            static const PROPERTYKEY friendlyName={{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};
            UINT count=0;collection->GetCount(&count);
            for(UINT i=0;i<count;++i) {
                IMMDevice *device=nullptr;
                if(FAILED(collection->Item(i,&device))||!device)continue;
                LPWSTR rawId=nullptr;QString id;
                if(SUCCEEDED(device->GetId(&rawId)) && rawId){id=QString::fromWCharArray(rawId);CoTaskMemFree(rawId);}
                QString name;
                IPropertyStore *store=nullptr;
                if(SUCCEEDED(device->OpenPropertyStore(STGM_READ,&store)) && store) {
                    PROPVARIANT value;PropVariantInit(&value);
                    if(SUCCEEDED(store->GetValue(friendlyName,&value)) && value.vt==VT_LPWSTR)
                        name=QString::fromWCharArray(value.pwszVal);
                    PropVariantClear(&value);store->Release();
                }
                if(name.isEmpty())name=id;
                if(!id.isEmpty())devices.append({id,name});
                device->Release();
            }
            collection->Release();
        }
        deviceEnumerator->Release();
    }
    if(SUCCEEDED(com))CoUninitialize();
    if(!devices.isEmpty())return devices;
#endif
    auto &api=vlc();
    if(!api.deviceSelectionAvailable())return devices;
    // Enumeration needs a media player; a short-lived one keeps the list
    // independent from the players that are currently on air.
    MediaPlayback enumerator;
    if(!enumerator.player)return devices;
    auto *list=api.audioDeviceEnum(enumerator.player);
    for(auto *entry=list;entry;entry=entry->next) {
        const QString id=QString::fromUtf8(entry->id?entry->id:"");
        QString name=QString::fromUtf8(entry->description?entry->description:"");
        if(name.isEmpty())name=id.isEmpty()?"Predeterminado":id;
        devices.append({id,name});
    }
    if(list)api.audioDeviceListRelease(list);
    return devices;
}
QString Multimedia::currentAudioDevice() const {
    auto &api=vlc();
    if(!playback || !playback->player || !api.audioDeviceGet)return appliedAudioDevice;
    char *id=api.audioDeviceGet(playback->player);
    const QString result=id?QString::fromUtf8(id):QString();
    if(id && api.freeMemory)api.freeMemory(id);
    return result;
}
void Multimedia::setAudioDevices(const QString &composition,const QString &output) {
    compositionAudioDevice=composition;outputAudioDevice=output;
    if(playback && playback->player)applyAudioDevice(cueMode?compositionAudioDevice:outputAudioDevice);
}
void Multimedia::applyAudioDevice(const QString &deviceId) {
    auto &api=vlc();
    if(!playback || !playback->player || !api.audioDeviceSet)return;
    if(audioDeviceApplied && appliedAudioDevice==deviceId)return;
    api.audioDeviceSet(playback->player,nullptr,deviceId.toUtf8().constData());
    appliedAudioDevice=deviceId;audioDeviceApplied=true;
}
bool Multimedia::volumeControlAvailable() { return vlc().volumeAvailable(); }
int Multimedia::currentAudioVolume() const {
    auto &api=vlc();
    if(playback && playback->player && api.audioGetVolume) {
        const int volume=api.audioGetVolume(playback->player);
        if(volume>=0)return volume;
    }
    return appliedVolume<0?100:appliedVolume;
}
void Multimedia::setAudioVolumes(int composition,int output) {
    compositionVolume=std::clamp(composition,0,100);
    outputVolume=std::clamp(output,0,100);
    if(playback && playback->player)applyAudioVolume(cueMode?compositionVolume:outputVolume);
}
void Multimedia::applyAudioVolume(int volume) {
    auto &api=vlc();
    if(!playback || !playback->player || !api.audioSetVolume)return;
    if(audioVolumeApplied && appliedVolume==volume)return;
    api.audioSetVolume(playback->player,volume);
    appliedVolume=volume;audioVolumeApplied=true;
}
bool Multimedia::purges() const {return purgeButton->isChecked();}
bool Multimedia::playsNext() const {return nextButton->isChecked();}
bool Multimedia::mixes() const {return mixButton->isChecked();}
void Multimedia::setPurge(bool enabled){purgeButton->setChecked(enabled && !imageMode);}
void Multimedia::setNext(bool enabled){nextButton->setChecked(enabled);}
void Multimedia::setMix(bool enabled){mixButton->setChecked(enabled);}
bool Multimedia::isImageFile(const QString &file) {
    return QImageReader::supportedImageFormats().contains(QFileInfo(file).suffix().toLower().toLatin1());
}
bool Multimedia::isAudioOnly() const {
    return QStringList{"mp3","wav","flac","ogg","opus","aac","m4a","wma","aif","aiff"}.contains(QFileInfo(path).suffix().toLower());
}
// Loop only applies to media with a timeline; still images stay visible until stopped.
void Multimedia::setLoop(bool enabled) {
    const bool active=enabled && !imageMode && QFileInfo(path).isFile();
    if(loopButton->isChecked()!=active)loopButton->setChecked(active);
    looping=active;
}
qint64 Multimedia::position() const {return finished?finishedTime:playback && playback->player?qMax(qint64(0),vlc().getTime(playback->player)):0;}
qint64 Multimedia::displayPosition() const {
    // VLC publishes its clock in coarse steps. Interpolate only the display;
    // playback, seeking and end detection keep using the real player clock.
    const qint64 sample=seeking?seekTarget:qMax(position(),shownPosition);
    if(finished || imageMode || seeking || !isPlaying()) {
        displayTick.invalidate();return finished?finishedTime:sample;
    }
    if(!displayTick.isValid()) {
        displaySample=sample;displayTime=sample;displayCorrection=0;displayTick.start();
    } else {
        const qint64 elapsed=displayTick.restart();
        displayTime+=elapsed;
        if(sample!=displaySample) {
            if(qAbs(sample-displaySample)>1000 || qAbs(sample-displayTime)>1000) {
                displayTime=sample;displayCorrection=0;
            } else displayCorrection=sample-displayTime;
            displaySample=sample;
        }
        const qreal correction=qBound(-elapsed*0.2,displayCorrection,elapsed*0.2);
        displayTime+=correction;displayCorrection-=correction;
        // A stalled player must not let the visual clock run indefinitely.
        displayTime=qBound(qreal(sample),displayTime,qreal(sample+500));
    }
    return qBound(qint64(0),qint64(displayTime),duration());
}
qint64 Multimedia::duration() const {
    if(playback && playback->player)knownDuration=qMax(knownDuration,vlc().getLength(playback->player));
    return knownDuration;
}
bool Multimedia::isPlaying() const {return imageMode?imageActive:playback && playback->player && vlc().getState(playback->player)==3;}
bool Multimedia::isPaused() const {return playback && playback->player && vlc().getState(playback->player)==4;}
bool Multimedia::hasVideo() const {return imageMode?!lastFrame.isNull():playback && playback->player && vlc().videoCount(playback->player)>0;}
bool Multimedia::isAudioMuted() const {return playback && playback->player && vlc().getMute(playback->player)==1;}
void Multimedia::configure(const QString &name,const QString &file) {
    thumbnailProbe.reset();updateThumbnail({});
    stop();playback.reset();frames.reset();lastFrame=QImage();firstFrame=QImage();mediaName=name.trimmed().isEmpty()?"Multimedia":name.trimmed();path=file;
    knownDuration=0;
    imageMode=isImageFile(path);
    pauseButton->setVisible(!imageMode);slider->setVisible(!imageMode);time->setVisible(!imageMode);
    loopButton->setVisible(!imageMode);
    purgeButton->setVisible(!imageMode);
    if(imageMode)purgeButton->setChecked(false);
    if(imageMode && loopButton->isChecked())loopButton->setChecked(false);
    loopButton->setEnabled(!imageMode && QFileInfo(path).isFile());
    title->setText(mediaName);title->setToolTip(mediaName);
    status->setText(path.isEmpty()?"Sin archivo":QFileInfo(path).isFile()?"Detenido":"Archivo no encontrado");
    status->setToolTip(path);playButton->setEnabled(QFileInfo(path).isFile());slider->setEnabled(false);slider->setValue(0);updateTime(0,0);
    cueButton->setEnabled(QFileInfo(path).isFile());
    mixButton->setEnabled(QFileInfo(path).isFile());
    playButton->setAccessibleName("Play · "+mediaName);
    const QString thumbnailPath=path;
    const auto generation=++preparationGeneration;
    preparingFile=QFileInfo(path).isFile();pendingTransport=0;setLoading(preparingFile);
    QTimer::singleShot(0,this,[this,thumbnailPath,generation]{
        if(generation!=preparationGeneration || path!=thumbnailPath)return;
        if(!QFileInfo(path).isFile() || playback) {
            preparingFile=false;pendingTransport=0;setLoading(false);return;
        }
        thumbnailProbe=std::make_unique<ThumbnailProbe>([this](const QImage &image,qint64 total){
            if(!image.isNull())updateThumbnail(image);
            knownDuration=qMax(knownDuration,total);updateTime(0,knownDuration);
            preparingFile=false;setLoading(false);
            // Leave the probe's completion callback before Play/Cue releases it.
            if(pendingTransport)QTimer::singleShot(0,this,[this]{
                if(preparingFile)return;
                const int requested=pendingTransport;pendingTransport=0;
                if(requested==1)play();else if(requested==2)cue();
            });
        });
        thumbnailProbe->start(path,isAudioOnly(),imageMode);
    });
    layout()->activate();updateGeometry();
    emit configurationChanged();
}
bool Multimedia::configureVideoPlayer(MediaPlayback &target,std::unique_ptr<VideoFrames> &buffer) {
    auto &api=vlc();
    if(!target.player)return false;
    auto *media=api.newMedia(target.instance,QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8().constData());
    if(!media)return false;
    // Keep the same demuxer and safe MOV decoder settings in both loop players.
    if(QStringList{"mp4","mov","m4v","m4a"}.contains(QFileInfo(path).suffix().toLower()))api.mediaOption(media,":demux=avformat");
    if(QFileInfo(path).suffix().compare("mov",Qt::CaseInsensitive)==0)api.mediaOption(media,":avcodec-threads=1");
    api.setMedia(target.player,media);api.releaseMedia(media);
    const auto generation=++frameGeneration;
    buffer=std::make_unique<VideoFrames>();buffer->serial=generation;
    buffer->notify=[this,generation]{QMetaObject::invokeMethod(this,[this,generation]{
        if(frames && frames->serial==generation)receiveFrame();
        else if(loopFrames && loopFrames->serial==generation)receiveLoopFrame();
    },Qt::QueuedConnection);};
    api.setCallbacks(target.player,VideoFrames::lock,VideoFrames::unlock,VideoFrames::display,buffer.get());
    api.setFormat(target.player,VideoFrames::format,nullptr);
    return true;
}
void Multimedia::prepareLoop() {
    if(!looping || imageMode || isAudioOnly() || !playback || !playback->player || loopReady || loopPreparing)return;
    loopPreparing=true;loopImage=QImage();
    auto &api=vlc();
    if(!loopPlayback) {
        loopPlayback=std::make_unique<MediaPlayback>();
        if(!configureVideoPlayer(*loopPlayback,loopFrames)){loopPlayback.reset();loopFrames.reset();return;}
        loopFrameFloor=1;
        api.mute(loopPlayback->player,1);
        if(api.audioDeviceSet)api.audioDeviceSet(loopPlayback->player,nullptr,(cueMode?compositionAudioDevice:outputAudioDevice).toUtf8().constData());
        if(api.play(loopPlayback->player)<0)return;
    } else {
        // The outgoing player is already paused. Seeking it now prepares the
        // first frame while the other player is showing the current iteration.
        api.mute(loopPlayback->player,1);
        const int state=api.getState(loopPlayback->player);
        loopFrameFloor=loopFrames->produced.load()+1;
        if(state==6 || state==5 || state==7) {
            // Restart an already-ended spare off the GUI thread. The other
            // decoder continues displaying frames while this one is prepared.
            if(loopPreparationThread)loopPreparationThread->wait();
            auto *player=loopPlayback->player;
            loopPreparationThread.reset(QThread::create([player]{vlc().stop(player);vlc().play(player);}));
            loopPreparationThread->start();
        } else {
            api.pause(loopPlayback->player,1);
            api.setTime(loopPlayback->player,0);
        }
    }
}
void Multimedia::receiveLoopFrame() {
    if(!loopFrames || !loopPlayback)return;
    loopFrames->queued=false;
    if(!loopPreparing || loopReady)return;
    if(loopFrames->produced.load()<loopFrameFloor)return;
    // Discard queued pictures from the end of the previous iteration.
    if(vlc().getTime(loopPlayback->player)>100)return;
    vlc().pause(loopPlayback->player,1);
    {QMutexLocker guard(&loopFrames->mutex);if(loopFrames->pixels)
        loopImage=QImage(loopFrames->pixels,int(loopFrames->width),int(loopFrames->height),int(loopFrames->pitch),QImage::Format_RGB32).copy();}
    loopReady=!loopImage.isNull();
    if(loopReady)loopPreparing=false;
}
bool Multimedia::activatePreparedLoop() {
    if(!loopReady || !loopPlayback || !loopFrames || loopImage.isNull())return false;
    if(loopPreparationThread && !loopPreparationThread->isFinished())return false;
    auto &api=vlc();
    api.mute(playback->player,1);api.pause(playback->player,1);
    playback.swap(loopPlayback);frames.swap(loopFrames);
    lastFrame=loopImage;loopImage=QImage();loopReady=false;loopPreparing=false;
    displayTick.invalidate();finished=false;finishedTime=0;shownPosition=0;seeking=false;
    acceptFrames=true;awaitingFreshFrame=false;
    audioDeviceApplied=false;audioVolumeApplied=false;
    applyAudioDevice(cueMode?compositionAudioDevice:outputAudioDevice);
    applyAudioVolume(cueMode?compositionVolume:outputVolume);
    api.mute(playback->player,cueMode?1:0);
    api.pause(playback->player,0);
    slider->setValue(slider->minimum());updateTime(0,duration());
    poll.start();timelineTimer.start();
    // Switch the picture immediately; no scene relaunch or fade at loop seams.
    emit frameReady(lastFrame);
    QTimer::singleShot(0,this,&Multimedia::prepareLoop);
    return true;
}
bool Multimedia::startPlayback() {
    if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))std::fprintf(stderr,"SESSIONS start cue=%d playing=%d finished=%d\n",cueMode,isPlaying(),finished);
    if(!QFileInfo(path).isFile()){setLoading(false);status->setText("Archivo no encontrado");return false;}
    if(imageMode) {
        QImageReader reader(path);reader.setAutoTransform(true);
        QImage image=reader.read();
        if(image.isNull()){setLoading(false);status->setText("No se pudo abrir la imagen");status->setToolTip(reader.errorString());return false;}
        lastFrame=image;firstFrame=image;imageActive=true;awaitingFreshFrame=false;
        status->setText(cueMode?"En composición":"En salida");
        playButton->setEnabled(cueMode);stopButton->setEnabled(true);
        emit frameReady(lastFrame);
        setLoading(false);
        return true;
    }
    if(!backendAvailable()){setLoading(false);status->setText("VLC 3 de 64 bits no disponible");return false;}
    if(!playback) {
        playback=std::make_unique<MediaPlayback>();
        if(!playback->player){setLoading(false);status->setText("No se pudo iniciar el reproductor");playback.reset();return false;}
        if(!configureVideoPlayer(*playback,frames)){setLoading(false);status->setText("No se pudo abrir el archivo");playback.reset();return false;}
        audioDeviceApplied=false;appliedAudioDevice.clear();
        audioVolumeApplied=false;appliedVolume=-1;
    }
    const bool audioOnly=isAudioOnly();
    vlc().mute(playback->player,cueMode && !audioOnly?1:0);
    if(finished){displayTick.invalidate();vlc().stop(playback->player);finished=false;finishedTime=0;shownPosition=0;seeking=false;awaitingFreshFrame=true;slider->setValue(slider->minimum());updateTime(0,duration());}
    applyAudioDevice(cueMode?compositionAudioDevice:outputAudioDevice);
    applyAudioVolume(cueMode?compositionVolume:outputVolume);
    acceptFrames=true;
    if(!isPlaying() && vlc().play(playback->player)<0){setLoading(false);acceptFrames=false;status->setText("Error al reproducir");return false;}
    status->setText("Cargando…");playButton->setEnabled(cueMode);stopButton->setEnabled(true);poll.start();timelineTimer.start();
    return true;
}
// Restart the same media without leaving the output, so a looping action never
// freezes on its last frame and the other composition elements stay in place.
void Multimedia::restartForLoop() {
    if(activatePreparedLoop())return;
    displayTick.invalidate();
    if(!playback || !playback->player)return;
    if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))std::fprintf(stderr,"SESSIONS loop restart\n");
    const qint64 total=duration();
    if(isPlaying() && vlc().seekable(playback->player)) {
        // Rewind the live input instead of tearing down and reopening VLC.
        // Keep the outgoing image until the first frame after the seek arrives.
        finished=false;finishedTime=0;shownPosition=0;seekTarget=0;seeking=true;seekWait.start();
        acceptFrames=true;awaitingFreshFrame=true;
        vlc().setTime(playback->player,0);
        slider->setValue(slider->minimum());updateTime(0,total);
        return;
    }
    // Fallback if EOF was reached before the live rewind, or seeking is unavailable.
    vlc().stop(playback->player);
    finished=false;finishedTime=0;shownPosition=0;seeking=false;acceptFrames=true;awaitingFreshFrame=true;
    slider->setValue(slider->minimum());updateTime(0,total);poll.start();
    if(vlc().play(playback->player)<0){acceptFrames=false;status->setText("Error al reproducir");poll.stop();timelineTimer.stop();return;}
    status->setText(cueMode?"En composición":"En marcha");
}
void Multimedia::play() {
    if(preparingFile){pendingTransport=1;setLoading(true);return;}
    pendingTransport=0;
    thumbnailProbe.reset();
    cueMode=false;
    if(startPlayback()){setTransportActive(playButton);setNext(false);emit playRequested();}
    else setTransportActive(nullptr);
}
void Multimedia::cue() {
    if(preparingFile){pendingTransport=2;setLoading(true);return;}
    pendingTransport=0;
    thumbnailProbe.reset();
    stopPlayback(false);playback.reset();frames.reset();lastFrame=QImage();firstFrame=QImage();cueMode=true;emit cueRequested();
    setTransportActive(startPlayback()?cueButton:nullptr);
}
void Multimedia::receiveFrame() {
    if(!frames || !playback)return;
    frames->queued=false;
    if(!acceptFrames || finished)return;
    {QMutexLocker guard(&frames->mutex);if(!frames->pixels)return;lastFrame=QImage(frames->pixels,int(frames->width),int(frames->height),int(frames->pitch),QImage::Format_RGB32).copy();}
    if(firstFrame.isNull()){firstFrame=lastFrame;if(thumbnailImage.isNull())updateThumbnail(lastFrame);}
    awaitingFreshFrame=false;
    setLoading(false);
    emit frameReady(lastFrame);
}
void Multimedia::pause(){
    displayTick.invalidate();
    if(imageMode){imageActive=false;setTransportActive(nullptr);playButton->setEnabled(QFileInfo(path).isFile());status->setText("Detenido");return;}
    if(isPlaying()){vlc().pause(playback->player,1);setTransportActive(pauseButton);QTimer::singleShot(50,this,&Multimedia::updatePlayback);}
}
void Multimedia::stop(){
    stopPlayback(true);
}
void Multimedia::stopPlayback(bool clearOutput){
    if(loopPreparationThread){loopPreparationThread->wait();loopPreparationThread.reset();}
    loopPreparing=false;loopReady=false;loopPlayback.reset();loopFrames.reset();loopImage=QImage();
    displayTick.invalidate();
    setTransportActive(nullptr);
    pendingTransport=0;
    setLoading(false);
    duration(); // Retain metadata before VLC releases its input.
    cueMode=false;imageActive=false;finished=false;finishedTime=0;acceptFrames=false;awaitingFreshFrame=true;shownPosition=0;seeking=false;
    poll.stop();timelineTimer.stop();if(playback && playback->player)vlc().stop(playback->player);
    if(clearOutput){playback.reset();frames.reset();}
    playButton->setEnabled(QFileInfo(path).isFile());pauseButton->setEnabled(false);stopButton->setEnabled(false);
    loopButton->setEnabled(!imageMode && QFileInfo(path).isFile());
    slider->setEnabled(false);slider->setValue(0);updateTime(0,duration());status->setText(path.isEmpty()?"Sin archivo":"Detenido");
    if(clearOutput)emit playbackStopped();
}
void Multimedia::seek(qint64 milliseconds){
    if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))std::fprintf(stderr,"SESSIONS seek target=%lld\n",static_cast<long long>(milliseconds));
    if(finished || !playback || !playback->player || !vlc().seekable(playback->player))return;
    displayTick.invalidate();
    seekTarget=qBound(qint64(0),milliseconds,duration());shownPosition=seekTarget;seeking=true;seekWait.start();
    vlc().setTime(playback->player,seekTarget);updatePlayback();
}
void Multimedia::updateTime(qint64 current,qint64 total){time->setText(clockTime(current)+" / "+clockTime(total));}
void Multimedia::updateTimeline() {
    if(slider->isSliderDown())return;
    const qint64 total=duration(),current=displayPosition();
    if(looping && loopReady && !finished && !seeking && playback && playback->player
        && ((isPlaying() && total>0 && current>=total-1) || vlc().getState(playback->player)==6)) {
        if(activatePreparedLoop())return;
    }
    if(looping && !loopReady && !imageMode && !finished && !seeking && total>100 && current>=total-20
        && isPlaying() && vlc().seekable(playback->player)) {
        restartForLoop();return;
    }
    slider->setValue(total>0?int(qMin(current,total)*slider->maximum()/total):0);
    updateTime(current,total);
}
void Multimedia::updatePlayback(){
    if(!playback || !playback->player)return;
    if(finished)return;
    int state=vlc().getState(playback->player);qint64 total=duration(),current=position();
    if(state==3 && hasVideo())prepareLoop();
    if(cueMode && state==3 && !hasVideo())vlc().mute(playback->player,0);
    // End-of-file may reset VLC's clock/length before its terminal state is read.
    // Rewind only on an explicit seek/Stop or once completion is confirmed.
    if(seeking) {
        const qint64 tolerance=qMin(qint64(100),qMax(qint64(10),total/4));
        if(qAbs(current-seekTarget)<=tolerance || seekWait.elapsed()>2000)seeking=false;
        else current=seekTarget;
    }
    if(state==6 || ((state==0 || state==5) && total>0 && shownPosition>=qMax(qint64(1),total-1000))) {
        // Loop restarts from the beginning on the same output instead of finishing.
        if(looping) { restartForLoop(); return; }
        state=6;finished=true;knownDuration=qMax(total,shownPosition);
        finishedTime=knownDuration;total=knownDuration;current=finishedTime;acceptFrames=false;seeking=false;
        vlc().stop(playback->player);
    } else if(state==3 || state==4) current=qMax(current,shownPosition);
    shownPosition=current;
    updateTimeline();
    slider->setEnabled(total>0 && (state==3 || state==4) && vlc().seekable(playback->player));
    playButton->setEnabled((cueMode && state==3) || (state!=1 && state!=2 && state!=3));pauseButton->setEnabled(state==3);stopButton->setEnabled(state==1 || state==2 || state==3 || state==4 || state==6);
    status->setText(state==3?(cueMode?"En composición":"En marcha"):state==4?"En pausa":state==6?"Finalizado":state==7?"Error de reproducción":state==1 || state==2?"Cargando…":"Detenido");
    if(state==6 || state==7 || state==5)poll.stop();
    if(state==4 || state==5 || state==6 || state==7)timelineTimer.stop();
    if(state==3)setTransportActive(cueMode?cueButton:playButton);
    else if(state==4)setTransportActive(pauseButton);
    else if(state==5 || state==6 || state==7)setTransportActive(nullptr);
    if(finished) {
        emit playbackFinished();
    }
}
void Multimedia::contextMenuEvent(QContextMenuEvent *event){emit contextMenuRequested(this,event->globalPos());event->accept();}
void Multimedia::showProperties(){
    QDialog dialog(this);dialog.setObjectName("MultimediaProperties");dialog.setWindowTitle("Propiedades de "+mediaName);
    auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;
    auto *nameEdit=new QLineEdit(mediaName);nameEdit->setObjectName("multimediaName");form->addRow("Nombre:",nameEdit);
    auto *fileEdit=new QLineEdit(path);fileEdit->setObjectName("multimediaFile");
    auto *row=new QHBoxLayout;row->addWidget(fileEdit,1);auto *browse=new QPushButton("Examinar…");row->addWidget(browse);form->addRow("Archivo:",row);layout->addLayout(form);
    connect(browse,&QPushButton::clicked,&dialog,[&]{QString file=QFileDialog::getOpenFileName(&dialog,"Abrir imagen, audio o vídeo",fileEdit->text(),"Multimedia (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff *.svg *.ico *.mp3 *.wav *.flac *.ogg *.aac *.m4a *.mp4 *.mkv *.avi *.mov *.webm *.wmv);;Todos los archivos (*)");if(!file.isEmpty())fileEdit->setText(file);});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(nameEdit->text().trimmed().isEmpty())return;if(!fileEdit->text().trimmed().isEmpty() && !QFileInfo(fileEdit->text().trimmed()).isFile()){QMessageBox::warning(&dialog,"Archivo","Selecciona un archivo existente.");return;}dialog.accept();});
    dialog.resize(490,150);if(dialog.exec()==QDialog::Accepted)configure(nameEdit->text(),fileEdit->text().trimmed().isEmpty()?QString():QFileInfo(fileEdit->text().trimmed()).absoluteFilePath());
}
