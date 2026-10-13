/**
******************************************************************************
* @file    av_control.c
* @author  盛图科技 vision.liu
* @version V1.1
* @date    init 20230816， 重构 20230821
* @brief   视频&音频控制，并于ui界面进行交互
******************************************************************************
*/
#include <QFileInfo>
#include <QDebug>
#include "av_control.h"

#define QUEUE_LENGTH            10

STAVControl *STAVControl::m_pInstance = NULL;

STAVControl::STAVControl() : m_sem1(QUEUE_LENGTH)
{
    FUNCTION_ENTER;
    m_iStatus = AV_S_INIT;
    m_pAudioOut = NULL;

    m_pAVFormatCtx = NULL;
    m_totalMs = 0;
    m_iIntervalVideoFrame = 0;

    m_VIndex = -1;
    m_AIndex = -1;
    m_pvCodecCtx = NULL;
    m_paCodecCtx = NULL;

    m_pVframe = NULL;
    m_pVframeRGB = NULL;
    m_pAframe = NULL;
    m_imgbuf = NULL;
    m_pSwsVCtx = NULL;
    m_pAVpkt = NULL;

    m_pSwsACtx = NULL;
    m_pAudioBuff = NULL;
    m_pIODev = NULL;

    m_listData.clear();
    m_pPreData = NULL;
    FUNCTION_EXIT;
}

STAVControl::~STAVControl()
{
    FUNCTION_ENTER;
    FUNCTION_EXIT;
}

STAVControl *STAVControl::GetInstance()
{
    FUNCTION_ENTER;
    if (NULL == m_pInstance)
    {
        m_pInstance = new STAVControl();
    }
    FUNCTION_EXIT;
    return m_pInstance;
}

void STAVControl::DestroyINstance()
{
    FUNCTION_ENTER;
    if (NULL != m_pInstance)
    {
        m_pInstance->ResetAVControl();
        delete m_pInstance;
        m_pInstance = NULL;
    }
    FUNCTION_EXIT;
}

int STAVControl::InitControl()
{
    FUNCTION_ENTER;
    int iRet = -1;

    avdevice_register_all();
    iRet = avformat_network_init();
    if (iRet != 0)
    {
        return CTL_INIT_ERR;
    }
    QAudioFormat format;
    format.setSampleRate(44100);     //设置采样率
    format.setChannelCount(2);        //设置通道数
    format.setSampleSize(16);        //样本数据16位
    format.setCodec("audio/pcm");        //播出格式为pcm格式
    format.setByteOrder(QAudioFormat::LittleEndian);  //默认小端模式
    format.setSampleType(QAudioFormat::UnSignedInt);    //无符号整形数

    QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
    if (!info.isFormatSupported(format))
    {
        qDebug()  <<"audio device don't play";
        return CTL_INIT_ERR;
    }
    m_pAudioOut = new QAudioOutput(format, this);
    m_pAudioOut->setBufferSize(100000);
    m_pAudioOut->setVolume(50);
    m_pAudioOut->stop();

    return CTL_OK;
}

int STAVControl::OpenAVFile(AvFileInfo info)
{
    FUNCTION_ENTER;
    if (ChenckFile(info.m_strUrl) != CTL_OK)
    {
        qDebug() << "check file info error";
        return CTL_FILENAME_ERR;
    }
    //获取音频与视频信息 step 1
    int iRet = InitAVStep_1(info);
    if ( iRet != CTL_OK)
    {
        qDebug() << "init_1 av file play info error";
        return iRet;
    }

    iRet = InitAVStep_2();
    if ( iRet != CTL_OK)
    {
        qDebug() << "init_2 av file play info error";

        goto INIT_STEP_2_ERR;
    }
    iRet = InitAVStep_3();
    if ( iRet != CTL_OK)
    {
        qDebug() << "init_3 av file play info error";

        goto INIT_STEP_3_ERR;
    }
    m_pIODev = m_pAudioOut->start();
    //启动线程
    m_iStatus = AV_S_INIT;
    this->start();
    qDebug() << "open file : " << info.m_strUrl;
    FUNCTION_EXIT;
    return CTL_OK;

INIT_STEP_3_ERR:
    swr_close(m_pSwsACtx);
    swr_free(&m_pSwsACtx);
    m_pSwsACtx = NULL;

    av_free(m_pAudioBuff);
    m_pAudioBuff = NULL;
    av_free(m_pAVpkt);
    m_pAVpkt = NULL;

    sws_freeContext(m_pSwsVCtx);
    m_pSwsVCtx = NULL;

    av_free(m_imgbuf);
    m_imgbuf = NULL;

    av_frame_free(&m_pVframe);
    av_frame_free(&m_pVframeRGB);
    av_frame_free(&m_pAframe);
    m_pAframe = NULL;
    m_pVframeRGB = NULL;
    m_pAframe = NULL;

INIT_STEP_2_ERR:
    avformat_close_input(&m_pAVFormatCtx);
    avformat_free_context(m_pAVFormatCtx);
    m_pAVFormatCtx = NULL;
    return CTL_INIT_ERR;
}

void STAVControl::StopPlay()
{
    FUNCTION_ENTER;
    m_iStatus = AV_S_END;
    int i = 15;

    while(i--)
    {
        if(!isRunning())
        {
            qDebug() << "av Thread exit";
            break;
        }
        //QTest::qSleep(30);
        QThread::msleep(30);
    }

    //m_listData.clear();
    if (m_sem1.available() < QUEUE_LENGTH)
    {
        m_sem1.release(QUEUE_LENGTH - m_sem1.available());
    }
    FUNCTION_EXIT;
}

AVData *STAVControl::GetData()
{
    FUNCTION_ENTER;
    AVData *tmp = NULL;
    if (m_pPreData != NULL)
    {
        delete m_pPreData;
        m_pPreData = NULL;
    }
    if (m_listData.length() > 0)
    {
        tmp = m_listData.first();
        m_pPreData = tmp;
        m_listData.pop_front();
        m_sem1.release(1);
        qDebug() <<"list : " << m_listData.length()  << "playtime : " << tmp->m_iPlayTime << " nexttime : " << tmp->m_iNextTime << " data type : " << tmp->m_iType << " sem: " << m_sem1.available();
    }
    FUNCTION_EXIT;
    return tmp;
}

void STAVControl::PausePlay()
{
    FUNCTION_ENTER;
    m_iStatus = AV_S_PAUSE;
    FUNCTION_EXIT;
}

void STAVControl::ContinuePlay()
{
    FUNCTION_ENTER;
    m_iStatus = AV_S_CONTINUE;
    FUNCTION_EXIT;
}

void STAVControl::SeekPlay(unsigned int ts)
{
    FUNCTION_ENTER;
    //int iRet = avformat_seek_file(m_pAVFormatCtx, -1, INT64_MIN, ts * AV_TIME_BASE, INT64_MAX, 0); s
    if(m_pAVFormatCtx == NULL)
    {
        return;
    }
    int iRet = avformat_seek_file(m_pAVFormatCtx, -1, INT64_MIN, ts * 1000, INT64_MAX, 0);   //ms
    if (iRet < 0)
    {
        qDebug() << "av seek error";
        return;
    }
    qDebug() << "av seek ok" << iRet;
    FUNCTION_EXIT;
}

void STAVControl::ResetAVControl()
{
    FUNCTION_ENTER;
    m_pAudioOut->bytesFree();
    if (NULL != m_pSwsACtx)
    {
        swr_close(m_pSwsACtx);
        swr_free(&m_pSwsACtx);
        m_pSwsACtx = NULL;
    }

    if (NULL != m_pAudioBuff)
    {
        av_free(m_pAudioBuff);
        m_pAudioBuff = NULL;
    }

    if (NULL != m_pAVpkt)
    {
        av_packet_unref(m_pAVpkt);
        av_packet_free(&m_pAVpkt);
        //av_free(m_pAVpkt);
        m_pAVpkt = NULL;
    }

    if (NULL != m_pSwsVCtx)
    {
        sws_freeContext(m_pSwsVCtx);
        m_pSwsVCtx = NULL;
    }

    if (NULL != m_imgbuf)
    {
        av_free(m_imgbuf);
        m_imgbuf = NULL;
    }

    if (NULL != m_pVframe)
    {
        av_frame_unref(m_pVframe);
        av_frame_free(&m_pVframe);
    }
    if (NULL != m_pVframeRGB)
    {
        av_frame_unref(m_pVframeRGB);
        av_frame_free(&m_pVframeRGB);
    }
    if (NULL != m_pAframe)
    {
        av_frame_unref(m_pAframe);
        av_frame_free(&m_pAframe);
    }
    m_pAframe = NULL;
    m_pVframeRGB = NULL;
    m_pAframe = NULL;

    if (NULL != m_pvCodecCtx)
    {
        avcodec_close(m_pvCodecCtx);
        avcodec_free_context(&m_pvCodecCtx);
        m_pvCodecCtx = NULL;
    }

    if (NULL != m_paCodecCtx)
    {
        avcodec_close(m_paCodecCtx);
        avcodec_free_context(&m_paCodecCtx);
        m_paCodecCtx = NULL;
    }

    if (NULL != m_pAVFormatCtx)
    {
        avformat_close_input(&m_pAVFormatCtx);
        avformat_free_context(m_pAVFormatCtx);
        m_pAVFormatCtx = NULL;
    }

    int i = 0;
    for(i = 0; i < m_listData.length(); i++)
    {
        AVData *tmp = m_listData[i];
        delete tmp;
    }
    m_listData.clear();
    if (m_sem1.available() < QUEUE_LENGTH)
    {
        m_sem1.release(QUEUE_LENGTH - m_sem1.available());
    }
    qDebug() << "list len : " << m_listData.length() << " m_sem : " << m_sem1.available();
    FUNCTION_EXIT;
}

unsigned int STAVControl::GetTotalMs()
{
    return m_totalMs;
}

void STAVControl::SetVolume(unsigned int val)
{
    FUNCTION_ENTER;
    if (val >= 0 && val <= 100)
    {
        m_pAudioOut->setVolume(val);
    }
    FUNCTION_EXIT;
}

int STAVControl::ChenckFile(QString strFile)
{
    FUNCTION_ENTER;
    if (strFile.length() <= 0)
    {
        return CTL_FILENAME_ERR;
    }
    //检查文件类型为mp4
    QFileInfo info(strFile);
    if (!info.isFile() || !info.isReadable() || info.completeSuffix() != "mp4")
    {
        return CTL_FILENAME_ERR;
    }
    FUNCTION_EXIT;
    return CTL_OK;
}

int STAVControl::InitAVStep_1(AvFileInfo info)
{
    FUNCTION_ENTER;
    if (info.m_strUrl.length() <= 0)
    {
        return CTL_FILENAME_ERR;
    }
    m_pAVFormatCtx = avformat_alloc_context();
    if (NULL == m_pAVFormatCtx)
    {
        qDebug() << "avformat_alloc_context error";
        return CTL_INIT_1_ERR;
    }
    //解密
    //AVDictionary *format_opts = NULL;
    //av_dict_set(&format_opts, "decryption_key", AV_KEY, 0);
    //if (avformat_open_input(&m_pAVFormatCtx, info.m_strUrl.toStdString().data(), 0, &format_opts) != 0)
    if (avformat_open_input(&m_pAVFormatCtx, info.m_strUrl.toStdString().data(), 0, 0) != 0)
    {
        qDebug() << "avformat_open_input : open file error";
        avformat_free_context(m_pAVFormatCtx);
        m_pAVFormatCtx = NULL;
        return CTL_INIT_1_ERR;
    }

    avformat_find_stream_info(m_pAVFormatCtx, NULL);
    m_totalMs = m_pAVFormatCtx->duration / 1000;
    qDebug() << "total time : " << m_totalMs << " duration :" << m_pAVFormatCtx->duration;

    int index = 0;
    for(index = 0; index < m_pAVFormatCtx->nb_streams; index++)
    {
        if (m_pAVFormatCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_VIndex = index;//视频流
            m_iIntervalVideoFrame = m_pAVFormatCtx->duration / m_pAVFormatCtx->streams[index]->nb_frames;
            qDebug() << "v fps : " << m_pAVFormatCtx->streams[index]->nb_frames <<"  " << m_iIntervalVideoFrame  << " duration_ts " << m_pAVFormatCtx->streams[index]->duration;
            //检测视频帧与播放时间是否与服务器视频信息一致，防止只替换文件名播放文件
            if (info.m_iVFrames != m_pAVFormatCtx->streams[index]->nb_frames
                || info.m_iVDurationTs != m_pAVFormatCtx->streams[index]->duration)
            {
                m_VIndex = -1;
            }

        }
        if (m_pAVFormatCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_AIndex = index;//音频流
            qDebug() << "a fps : " << m_pAVFormatCtx->streams[index]->nb_frames  << " duration_ts " << m_pAVFormatCtx->streams[index]->duration;
            //检测音频帧与播放时间是否与服务器音频信息一致，防止只替换文件名播放文件
            if (info.m_iAFrames != m_pAVFormatCtx->streams[index]->nb_frames
                || info.m_iADurationTs !=  m_pAVFormatCtx->streams[index]->duration)
            {
                m_AIndex = -1;
            }
        }
    }
    if(m_VIndex == -1 || m_AIndex == -1)
    {
        qDebug() << "not find video and audio";
        avformat_close_input(&m_pAVFormatCtx);
        avformat_free_context(m_pAVFormatCtx);
        m_pAVFormatCtx = NULL;
        m_iIntervalVideoFrame = 0;
        return CTL_INIT_1_ERR;
    }
    FUNCTION_EXIT;
    return CTL_OK;
}

int STAVControl::InitAVStep_2()
{
    FUNCTION_ENTER;
    int iRet = CTL_INIT_2_ERR;
    m_pvCodecCtx = avcodec_alloc_context3(NULL);
    m_paCodecCtx = avcodec_alloc_context3(NULL);
    if(NULL == m_pvCodecCtx || NULL == m_paCodecCtx)
    {
        qDebug() << "alloc a / v codec context3 error";
        avcodec_free_context(&m_pvCodecCtx);
        m_pvCodecCtx = NULL;
        avcodec_free_context(&m_paCodecCtx);
        m_paCodecCtx = NULL;
        return CTL_INIT_2_ERR;
    }
    //查找视频解码流
    iRet= avcodec_parameters_to_context(m_pvCodecCtx, m_pAVFormatCtx->streams[m_VIndex]->codecpar);
    if(iRet != 0)
    {
        goto FIND_V_CONTEXT_ERR;
    }
    m_pVideoCodec = avcodec_find_decoder(m_pvCodecCtx->codec_id);
    if (NULL == m_pVideoCodec)
    {
        goto FIND_V_DECODER_ERR;
    }
    //查找音频解码器
    iRet= avcodec_parameters_to_context(m_paCodecCtx, m_pAVFormatCtx->streams[m_AIndex]->codecpar);
    if(iRet != 0)
    {
        goto FIND_A_CONTEXT_ERR;
    }
    m_pAudioCodec = avcodec_find_decoder(m_paCodecCtx->codec_id);
    if (NULL == m_pAudioCodec)
    {
        goto FIND_A_DECODER_ERR;
    }
    //
    if (avcodec_open2(m_pvCodecCtx, m_pVideoCodec, 0) < 0)
    {
        qDebug() << "video avctx The context to initialize error";
        goto INIT_VIDEO_AVCTX_ERR;
    }

    if (avcodec_open2(m_paCodecCtx, m_pAudioCodec, 0) < 0)
    {
        qDebug() << "audio avctx The context to initialize error";
        goto INIT_AUDIO_AVCTX_ERR;
    }
    FUNCTION_EXIT;
    return CTL_OK;

INIT_AUDIO_AVCTX_ERR:
    avcodec_close(m_pvCodecCtx);
INIT_VIDEO_AVCTX_ERR:
//未找到音频解码上下文也解码器
FIND_A_DECODER_ERR:
FIND_A_CONTEXT_ERR:
//为找到视频解码上下文与解码器
FIND_V_DECODER_ERR:
FIND_V_CONTEXT_ERR:
    m_pAudioCodec = NULL;
    m_pVideoCodec = NULL;
    avcodec_free_context(&m_pvCodecCtx);
    m_pvCodecCtx = NULL;
    avcodec_free_context(&m_paCodecCtx);
    m_paCodecCtx = NULL;

    return CTL_INIT_2_ERR;
}

int STAVControl::InitAVStep_3()
{
    FUNCTION_ENTER;
    int iRet = -1;
    int iPktBufSize = -1;

    m_pVframe = av_frame_alloc();
    m_pVframeRGB = av_frame_alloc();
    m_pAframe = av_frame_alloc();
    if (NULL == m_pVframe || NULL == m_pVframeRGB || NULL== m_pAframe)
    {
        goto ALLOC_FRAME_ERROR;
    }

    //初始化视频部分
    //获取视频转图片需要内存大小，单位字节
    iPktBufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_pvCodecCtx->width, m_pvCodecCtx->height, 1);
    qDebug() << "image get buff size " << iPktBufSize;
    if (iPktBufSize <= 0)
    {
        qDebug() << "image get buff size error";
        goto GET_IMAGE_BUF_SIZE;
    }

    m_imgbuf = (unsigned char *)av_malloc(iPktBufSize); //注意使用ffmpeg库函数申请
    if (NULL == m_imgbuf)
    {
        goto MALLOC_IMG_BUF_SIZE_ERR;
    }

    //将输入图像数据转换为指定的输出图像格式，并填充到AVFrame结构中
    iRet = av_image_fill_arrays(m_pVframeRGB->data, m_pVframeRGB->linesize, m_imgbuf, AV_PIX_FMT_RGB32, m_pvCodecCtx->width, m_pvCodecCtx->height, 1);
    qDebug() << "av image fill arrays " << iRet;
    if (iRet <= 0)
    {
        goto FILL_IMG_BUF_ERR;
    }

    //转换后图片大小为原始大小
    m_pSwsVCtx = sws_getContext(m_pvCodecCtx->width, m_pvCodecCtx->height, m_pvCodecCtx->pix_fmt,
                m_pvCodecCtx->width, m_pvCodecCtx->height, AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);
    if (NULL == m_pSwsVCtx)
    {
        goto SWS_GET_VIDEO_CTX_ERR;
    }

    //创建音视频读取av_read_frame填充包
    m_pAVpkt = (AVPacket *)av_malloc(sizeof(AVPacket));
    if (NULL == m_pAVpkt)
    {
        goto MALLOC_AVPKT_ERR;
    }

    //音频部分初始化
    m_pAudioBuff = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    if (NULL == m_pAudioBuff)
    {
        goto MALLOC_AUDIO_BUF_ERROR;
    }
    memset(m_pAudioBuff, 0, MAX_AUDIO_FRAME_SIZE);
    //设置音频上下文转换参数
    m_pSwsACtx = swr_alloc_set_opts(m_pSwsACtx, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16, 44100,
                    m_paCodecCtx->channel_layout, m_paCodecCtx->sample_fmt,m_paCodecCtx->sample_rate, 0, 0);
    if (NULL == m_pSwsACtx)
    {
        goto SWS_GET_AUDIO_CTX_ERR;
    }
    //初始化
    iRet= swr_init(m_pSwsACtx);
    if (iRet != 0)
    {
        qDebug() << "swr_init error " << iRet;
        goto SWR_INIT_AUDIO_CTX_ERR;
    }
    FUNCTION_EXIT;
    return CTL_OK;

SWR_INIT_AUDIO_CTX_ERR:
    swr_close(m_pSwsACtx);
    swr_free(&m_pSwsACtx);
    m_pSwsACtx = NULL;

SWS_GET_AUDIO_CTX_ERR:
    av_free(m_pAudioBuff);
    m_pAudioBuff = NULL;
    av_free(m_pAVpkt);
    m_pAVpkt = NULL;

MALLOC_AUDIO_BUF_ERROR:
MALLOC_AVPKT_ERR:
    sws_freeContext(m_pSwsVCtx);
    m_pSwsVCtx = NULL;

SWS_GET_VIDEO_CTX_ERR:
FILL_IMG_BUF_ERR:
    av_free(m_imgbuf);
    m_imgbuf = NULL;

MALLOC_IMG_BUF_SIZE_ERR:
GET_IMAGE_BUF_SIZE:
ALLOC_FRAME_ERROR:
    av_frame_free(&m_pVframe);
    av_frame_free(&m_pVframeRGB);
    av_frame_free(&m_pAframe);
    m_pAframe = NULL;
    m_pVframeRGB = NULL;
    m_pAframe = NULL;
    return CTL_INIT_3_ERR;
}

void STAVControl::PushAVData(AVData *pData)
{
    FUNCTION_ENTER;
    if (NULL == pData)
    {
        return;
    }
    if(m_sem1.available() == 0 && m_iStatus ==  AV_S_INIT)
    {
        m_iStatus = AV_S_PLAY;
        //emit StartPlay();
    }
    m_sem1.acquire(1);
    /*
    int i = m_listData.length();
    if (i > 0)
    {
         m_listData[i-1]->m_iNextTime = pData->m_iPlayTime;
    }*/
    m_listData.push_back(pData);

    FUNCTION_EXIT;
}

void STAVControl::run()
{
    FUNCTION_ENTER;
    int i = 0;
    int j = 0;
    int iRet= -1;
    while(1)
    {
        if (m_iStatus == AV_S_END)
        {
            break;
        }
        if (m_iStatus == AV_S_PAUSE)
        {
            //QTest::qSleep(100);
            QThread::msleep(100);
            continue;
        }
        iRet = av_read_frame(m_pAVFormatCtx, m_pAVpkt);
        //qDebug() << "read frame " << iRet << " " << m_pAVpkt->stream_index;
        if(iRet == 0)
        {
            if (m_pAVpkt->stream_index == m_VIndex)
            {
                iRet = avcodec_send_packet(m_pvCodecCtx, m_pAVpkt);
                if (iRet != 0)
                {
                    qDebug() << "v avcodec_send_packet " << iRet;
                    av_frame_unref(m_pVframe);
                    av_packet_unref(m_pAVpkt);
                    break;
                }
                iRet = avcodec_receive_frame(m_pvCodecCtx, m_pVframe);
                if (iRet != 0)
                {
                    qDebug() << "v avcodec_receive_frame " << iRet;
                    av_frame_unref(m_pVframe);
                    av_packet_unref(m_pAVpkt);
                    continue;
                }
                //从零开始到当前时间的毫秒数
                int current = av_q2d(m_pAVFormatCtx->streams[m_VIndex]->time_base)*1000 * m_pVframe->pts;
                qDebug() << "receive video frame ok " << j << " " << iRet <<" "<< current  <<" " << m_sem1.available();
                int height = sws_scale(m_pSwsVCtx, (const unsigned char* const*)m_pVframe->data, m_pVframe->linesize, 0, m_pvCodecCtx->height,
                          m_pVframeRGB->data,  m_pVframeRGB->linesize);
                QImage  *tmpImg  = new QImage((uchar *)m_imgbuf, m_pvCodecCtx->width, m_pvCodecCtx->height, QImage::Format_RGB32);

                AVData * pData = new AVData();
                pData->m_iPlayTime = current;//当前帧播放时间
                //pData->m_iNextTime = 0;
                pData->m_iType = AV_TYPE_VIDEO;
                pData->m_image = tmpImg;
                PushAVData(pData);

                av_frame_unref(m_pVframe);
                av_packet_unref(m_pAVpkt);

                emit StartPlay(AV_TYPE_VIDEO,current,j,AV_S_PLAY);
                j++;

            }
            else if (m_pAVpkt->stream_index == m_AIndex)//音频
            {

                avcodec_send_packet(m_paCodecCtx, m_pAVpkt);
                if (iRet != 0)
                {
                    qDebug() << "a avcodec_send_packet " << iRet;
                    av_frame_unref(m_pVframe);
                    av_packet_unref(m_pAVpkt);
                    break;
                }
                iRet = avcodec_receive_frame(m_paCodecCtx, m_pAframe);
                if (iRet != 0)
                {
                    qDebug() << "a avcodec_receive_frame " << iRet;
                    av_frame_unref(m_pAframe);
                    av_packet_unref(m_pAVpkt);
                    continue;
                }
                memset(m_pAudioBuff, 0, MAX_AUDIO_FRAME_SIZE);
                iRet = swr_convert(m_pSwsACtx, &m_pAudioBuff, MAX_AUDIO_FRAME_SIZE,
                       (const uint8_t **)m_pAframe->data, m_pAframe->nb_samples);
                int out_size=m_pAframe->nb_samples * 2 * 2;
                int current = av_q2d(m_pAVFormatCtx->streams[m_AIndex]->time_base)*1000 * m_pAframe->pts;
                qDebug() << "receive audio frame ok " << i << " " << iRet <<" "<< current << " " <<out_size << " "<< m_sem1.available();

                /*AVData * pData = new AVData();
                pData->m_iPlayTime = current;
                pData->m_iNextTime = 0;
                pData->m_iType = AV_TYPE_AUDIO;
                pData->m_image = NULL;
                pData->out_size = out_size;

                pData->m_pAudioBuff = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
                memset(pData->m_pAudioBuff, 0, MAX_AUDIO_FRAME_SIZE);
                memcpy(pData->m_pAudioBuff, m_pAudioBuff, MAX_AUDIO_FRAME_SIZE);
                PushAVData(pData);*/
                //i++;
                //播放
                while(m_pAudioOut->bytesFree() < out_size)
                {
                    qDebug() << "play sleep";
                    //QTest::qSleep(10);
                    QThread::msleep(10);
                }//播放
                m_pIODev->write((char*)m_pAudioBuff,out_size);
                av_frame_unref(m_pAframe);
                qDebug() << "write audio";
                emit StartPlay(AV_TYPE_AUDIO,current,i,AV_S_PLAY);
                i++;

            }

            av_packet_unref(m_pAVpkt);


        }
        else
        {
            av_packet_unref(m_pAVpkt);
            break;
        }
    }
    av_frame_unref(m_pVframe);
    av_frame_unref(m_pAframe);
    av_packet_unref(m_pAVpkt);
    qDebug() <<"exit";
    FUNCTION_EXIT;
}
