#include "multimedia.h"
#include <QtWidgets>
#include "slider.h"
#include <atomic>
#include <cstring>
#include <functional>
#include <cstdio>

struct VideoFrames {
    QMutex mutex;
    QByteArray storage;
    unsigned width=0,height=0,pitch=0;
    uchar *pixels=nullptr;
    std::atomic_bool queued{false};
    std::function<void()> notify;
    static unsigned format(void **opaque,char *chroma,unsigned *width,unsigned *height,unsigned *pitches,unsigned *lines) {
        auto *frames=static_cast<VideoFrames *>(*opaque);QMutexLocker guard(&frames->mutex);
        QSize size{int(*width),int(*height)};
        if(size.width()>1920 || size.height()>1080)size.scale(1920,1080,Qt::KeepAspectRatio);
        *width=unsigned(qMax(1,size.width()));*height=unsigned(qMax(1,size.height()));
        std::memcpy(chroma,"RV32",4);frames->width=*width;frames->height=*height;frames->pitch=(*width*4+31)&~31u;
        frames->storage.resize(qsizetype(frames->pitch)*frames->height+31);
        frames->pixels=reinterpret_cast<uchar *>((reinterpret_cast<quintptr>(frames->storage.data())+31)&~quintptr(31));
        *pitches=frames->pitch;*lines=frames->height;return 1;
    }
    static void *lock(void *opaque,void **planes) {
        auto *frames=static_cast<VideoFrames *>(opaque);frames->mutex.lock();*planes=frames->pixels;return nullptr;
    }
    static void unlock(void *opaque,void *,void *const *) {static_cast<VideoFrames *>(opaque)->mutex.unlock();}
    static void display(void *opaque,void *) {
        auto *frames=static_cast<VideoFrames *>(opaque);
        if(!frames->queued.exchange(true))frames->notify();
    }
};

// LibVLC 3 public C ABI. Resolve dynamically so the rest of Sessions works
// without VLC, and never load libraries from the current working directory.
namespace {
struct VlcApi {
    QLibrary core, library;
    void *(*create)(int,const char *const *) = nullptr;
    void (*release)(void *) = nullptr;
    void *(*newPlayer)(void *) = nullptr;
    void (*releasePlayer)(void *) = nullptr;
    void *(*newMedia)(void *,const char *) = nullptr;
    void (*releaseMedia)(void *) = nullptr;
    void (*mediaOption)(void *,const char *) = nullptr;
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
    bool ready = false;
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
                && resolve(mute,"libvlc_audio_set_mute");
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
Multimedia::Multimedia(QWidget *parent):QWidget(parent) {
    setObjectName("Multimedia"); setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(8,6,8,6); layout->setSpacing(6);
    auto *header=new QHBoxLayout;
    title=new QLabel(mediaName); title->setObjectName("MultimediaTitle");title->setTextFormat(Qt::PlainText);
    title->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);header->addWidget(title,1);
    status=new QLabel("Sin archivo");status->setObjectName("MultimediaStatus");header->addWidget(status);layout->addLayout(header);
    auto *controls=new QHBoxLayout;
    playButton=new QPushButton(QIcon(":/icons/Play.svg"),"Play");playButton->setObjectName("multimediaPlay");
    pauseButton=new QPushButton(QIcon(":/icons/Pause.svg"),"Pause");pauseButton->setObjectName("multimediaPause");
    stopButton=new QPushButton(QIcon(":/icons/Stop.svg"),"Stop");stopButton->setObjectName("multimediaStop");
    cueButton=new QPushButton(QIcon(":/icons/PlayCue.svg"),"Cue");cueButton->setObjectName("multimediaCue");cueButton->setEnabled(false);
    for(auto *button:{cueButton,playButton,pauseButton,stopButton})controls->addWidget(button);
    controls->addStretch();layout->addLayout(controls);
    slider=new Slider;slider->setObjectName("multimediaSlider");slider->setFixedHeight(26);slider->setEnabled(false);layout->addWidget(slider);
    time=new QLabel;time->setObjectName("MultimediaTime");time->setAlignment(Qt::AlignRight);layout->addWidget(time);updateTime(0,0);
    playButton->setEnabled(false);pauseButton->setEnabled(false);stopButton->setEnabled(false);
    connect(playButton,&QPushButton::clicked,this,&Multimedia::play);
    connect(cueButton,&QPushButton::clicked,this,&Multimedia::cue);
    connect(pauseButton,&QPushButton::clicked,this,&Multimedia::pause);
    connect(stopButton,&QPushButton::clicked,this,&Multimedia::stop);
    connect(slider,&QSlider::sliderMoved,this,[this](int value){updateTime(duration()*value/slider->maximum(),duration());});
    connect(slider,&QSlider::sliderReleased,this,[this]{seek(duration()*slider->value()/slider->maximum());});
    connect(slider,&QSlider::actionTriggered,this,[this](int){if(!slider->isSliderDown())QTimer::singleShot(0,this,[this]{seek(duration()*slider->value()/slider->maximum());});});
    poll.setInterval(100);connect(&poll,&QTimer::timeout,this,&Multimedia::updatePlayback);
    setToolTip("Botón derecho: archivo y propiedades de Multimedia");
}
Multimedia::~Multimedia() { poll.stop(); playback.reset(); }
bool Multimedia::backendAvailable(){return vlc().ready;}
bool Multimedia::isImageFile(const QString &file) {
    return QImageReader::supportedImageFormats().contains(QFileInfo(file).suffix().toLower().toLatin1());
}
qint64 Multimedia::position() const {return finished?finishedTime:playback && playback->player?qMax(qint64(0),vlc().getTime(playback->player)):0;}
qint64 Multimedia::duration() const {
    if(playback && playback->player)knownDuration=qMax(knownDuration,vlc().getLength(playback->player));
    return knownDuration;
}
bool Multimedia::isPlaying() const {return imageMode?imageActive:playback && playback->player && vlc().getState(playback->player)==3;}
bool Multimedia::isPaused() const {return playback && playback->player && vlc().getState(playback->player)==4;}
bool Multimedia::hasVideo() const {return imageMode?!lastFrame.isNull():playback && playback->player && vlc().videoCount(playback->player)>0;}
void Multimedia::configure(const QString &name,const QString &file) {
    stop();playback.reset();frames.reset();lastFrame=QImage();firstFrame=QImage();mediaName=name.trimmed().isEmpty()?"Multimedia":name.trimmed();path=file;
    knownDuration=0;
    imageMode=isImageFile(path);
    pauseButton->setVisible(!imageMode);slider->setVisible(!imageMode);time->setVisible(!imageMode);
    title->setText(mediaName);title->setToolTip(mediaName);
    status->setText(path.isEmpty()?"Sin archivo":QFileInfo(path).isFile()?"Detenido":"Archivo no encontrado");
    status->setToolTip(path);playButton->setEnabled(QFileInfo(path).isFile());slider->setEnabled(false);slider->setValue(0);updateTime(0,0);
    cueButton->setEnabled(QFileInfo(path).isFile());
    layout()->activate();updateGeometry();
    emit configurationChanged();
}
bool Multimedia::startPlayback() {
    if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))std::fprintf(stderr,"SESSIONS start cue=%d playing=%d finished=%d\n",cueMode,isPlaying(),finished);
    if(!QFileInfo(path).isFile()){status->setText("Archivo no encontrado");return false;}
    if(imageMode) {
        QImageReader reader(path);reader.setAutoTransform(true);
        QImage image=reader.read();
        if(image.isNull()){status->setText("No se pudo abrir la imagen");status->setToolTip(reader.errorString());return false;}
        lastFrame=image;firstFrame=image;imageActive=true;awaitingFreshFrame=false;
        status->setText(cueMode?"En composición":"En salida");
        playButton->setEnabled(cueMode);stopButton->setEnabled(true);
        emit frameReady(lastFrame);
        return true;
    }
    if(!backendAvailable()){status->setText("VLC 3 de 64 bits no disponible");return false;}
    if(!playback) {
        playback=std::make_unique<MediaPlayback>();
        if(!playback->player){status->setText("No se pudo iniciar el reproductor");playback.reset();return false;}
        auto &api=vlc();auto *media=api.newMedia(playback->instance,QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toUtf8().constData());
        if(!media){status->setText("No se pudo abrir el archivo");playback.reset();return false;}
        // VLC's native MP4 demuxer can reset PCR at fragment boundaries and
        // freeze the last decoded frame while its new clock catches up.
        // FFmpeg's demuxer preserves the timeline of fragmented MP4/MOV files.
        if(QStringList{"mp4","mov","m4v","m4a"}.contains(QFileInfo(path).suffix().toLower()))api.mediaOption(media,":demux=avformat");
        api.setMedia(playback->player,media);api.releaseMedia(media);
        const auto generation=++frameGeneration;
        frames=std::make_unique<VideoFrames>();frames->notify=[this,generation]{QMetaObject::invokeMethod(this,[this,generation]{if(generation==frameGeneration)receiveFrame();},Qt::QueuedConnection);};
        api.setCallbacks(playback->player,VideoFrames::lock,VideoFrames::unlock,VideoFrames::display,frames.get());
        api.setFormat(playback->player,VideoFrames::format,nullptr);
    }
    vlc().mute(playback->player,cueMode?1:0);
    if(finished){vlc().stop(playback->player);finished=false;finishedTime=0;shownPosition=0;seeking=false;awaitingFreshFrame=true;slider->setValue(slider->minimum());updateTime(0,duration());}
    acceptFrames=true;
    if(!isPlaying() && vlc().play(playback->player)<0){acceptFrames=false;status->setText("Error al reproducir");return false;}
    status->setText("Cargando…");playButton->setEnabled(cueMode);stopButton->setEnabled(true);poll.start();
    return true;
}
void Multimedia::play() {
    cueMode=false;
    if(startPlayback())emit playRequested();
}
void Multimedia::cue() {
    stopPlayback(false);playback.reset();frames.reset();lastFrame=QImage();firstFrame=QImage();cueMode=true;emit cueRequested();
    startPlayback();
}
void Multimedia::receiveFrame() {
    if(!frames || !playback)return;
    frames->queued=false;
    if(!acceptFrames || finished)return;
    {QMutexLocker guard(&frames->mutex);if(!frames->pixels)return;lastFrame=QImage(frames->pixels,int(frames->width),int(frames->height),int(frames->pitch),QImage::Format_RGB32).copy();}
    if(firstFrame.isNull())firstFrame=lastFrame;
    awaitingFreshFrame=false;
    emit frameReady(lastFrame);
}
void Multimedia::pause(){
    if(imageMode){imageActive=false;playButton->setEnabled(QFileInfo(path).isFile());status->setText("Detenido");return;}
    if(isPlaying()){vlc().pause(playback->player,1);QTimer::singleShot(50,this,&Multimedia::updatePlayback);}
}
void Multimedia::stop(){
    stopPlayback(true);
}
void Multimedia::stopPlayback(bool clearOutput){
    duration(); // Retain metadata before VLC releases its input.
    cueMode=false;imageActive=false;finished=false;finishedTime=0;acceptFrames=false;awaitingFreshFrame=true;shownPosition=0;seeking=false;
    poll.stop();if(playback && playback->player)vlc().stop(playback->player);
    if(clearOutput){playback.reset();frames.reset();}
    playButton->setEnabled(QFileInfo(path).isFile());pauseButton->setEnabled(false);stopButton->setEnabled(false);
    slider->setEnabled(false);slider->setValue(0);updateTime(0,duration());status->setText(path.isEmpty()?"Sin archivo":"Detenido");
    if(clearOutput)emit playbackStopped();
}
void Multimedia::seek(qint64 milliseconds){
    if(qEnvironmentVariableIsSet("SESSIONS_MEDIA_TRACE"))std::fprintf(stderr,"SESSIONS seek target=%lld\n",static_cast<long long>(milliseconds));
    if(finished || !playback || !playback->player || !vlc().seekable(playback->player))return;
    seekTarget=qBound(qint64(0),milliseconds,duration());shownPosition=seekTarget;seeking=true;seekWait.start();
    vlc().setTime(playback->player,seekTarget);updatePlayback();
}
void Multimedia::updateTime(qint64 current,qint64 total){time->setText(clockTime(current)+" / "+clockTime(total));}
void Multimedia::updatePlayback(){
    if(!playback || !playback->player)return;
    if(finished)return;
    int state=vlc().getState(playback->player);qint64 total=duration(),current=position();
    // End-of-file may reset VLC's clock/length before its terminal state is read.
    // Rewind only on an explicit seek/Stop or once completion is confirmed.
    if(seeking) {
        if(qAbs(current-seekTarget)<=500 || seekWait.elapsed()>2000)seeking=false;
        else current=seekTarget;
    }
    if(state==6 || ((state==0 || state==5) && total>0 && shownPosition>=qMax(qint64(1),total-1000))) {
        state=6;finished=true;knownDuration=qMax(total,shownPosition);
        finishedTime=knownDuration;total=knownDuration;current=finishedTime;acceptFrames=false;seeking=false;
        vlc().stop(playback->player);
    } else if(state==3 || state==4) current=qMax(current,shownPosition);
    shownPosition=current;
    if(!slider->isSliderDown()) {
        slider->setValue(total>0?int(qMin(current,total)*slider->maximum()/total):0);
        updateTime(current,total);
    }
    slider->setEnabled(total>0 && (state==3 || state==4) && vlc().seekable(playback->player));
    playButton->setEnabled((cueMode && state==3) || (state!=1 && state!=2 && state!=3));pauseButton->setEnabled(state==3);stopButton->setEnabled(state==1 || state==2 || state==3 || state==4 || state==6);
    status->setText(state==3?(cueMode?"En composición":"En marcha"):state==4?"En pausa":state==6?"Finalizado":state==7?"Error de reproducción":state==1 || state==2?"Cargando…":"Detenido");
    if(state==6 || state==7 || state==5)poll.stop();
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
