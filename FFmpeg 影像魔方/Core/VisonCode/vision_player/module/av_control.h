
#ifndef _AV_CONTROL_H_
#define _AV_CONTROL_H_

#include <QThread>
#include <QAudioOutput>
#include <QList>
//#include <QTest>
#include <QSemaphore>

#include "common.h"

enum AV_STATUS
{
    AV_S_INIT = 0,              //初始化
    AV_S_PLAY,                  //播放
    AV_S_END,                   //播放结束
    AV_S_PAUSE,                 //暂停播放
    AV_S_CONTINUE,              //继续播放
};

class STAVControl : public QThread
{
    Q_OBJECT
public:
    static STAVControl *GetInstance();
    static void DestroyINstance();

    int InitControl();                              //初始化音频输出与ffmpeg相关注册
    int OpenAVFile(AvFileInfo info);                //打开文件并播放
    void StopPlay();                                //停止播放
    AVData *GetData();                              //获取视频帧数据
    void PausePlay();                               //暂停
    void ContinuePlay();                            //继续播放
    void SeekPlay(unsigned int ts);                 //seek   单位秒
    void ResetAVControl();                          //打开文件播放前，停止播放，重置，然后重新打开
    unsigned int GetTotalMs();
    void SetVolume(unsigned int val);

signals:
   // void StartPlay(void);                           //开始播放和定位后重新播放发出此信号
                 // 数据帧类型     当前帧播放时间    索引           类型
    void StartPlay(int iType,quint64 playtime,quint32 index,int avType);//每读取一帧发一次信号
private:
    STAVControl();
    ~STAVControl();

    int ChenckFile(QString strFile);
    int InitAVStep_1(AvFileInfo info);              //初始化m_pAVFormatCtx、音频与视频流索引
    int InitAVStep_2();                             //初始化音视频解码器和解码上下文
    int InitAVStep_3();                             //初始化线程start前，创建与初始化音视频frame、packet、buf等相关资源
    void PushAVData(AVData *pData);

protected:
    void run();

private:
    int m_iStatus;
    QAudioOutput    *m_pAudioOut;                   //音频输出
    QIODevice       *m_pIODev;                      //音频写入

    //step 1 init start
    AVFormatContext *m_pAVFormatCtx;                //通过此结构体获取AV编解码上context及解码器
    unsigned int    m_totalMs;                      //视频总时长
    unsigned int    m_iIntervalVideoFrame;          //视频帧间隔时间,单位微秒
    int             m_VIndex;                       //视频流索引
    int             m_AIndex;                       //音频流索引
    //step 1 init end

    //step 2 init start
    AVCodecContext  *m_pvCodecCtx;                  //视频编解码上下文
    AVCodecContext  *m_paCodecCtx;                  //音频编解码上下文
    const AVCodec   *m_pVideoCodec;                 //视频流解码器
    const AVCodec   *m_pAudioCodec;                 //音频流解码器
    //step 2 init end

    //step 3 init start
    unsigned char   *m_imgbuf;                      //视频转图片buf
    AVFrame         *m_pVframe;                     //视频解码输入帧
    AVFrame         *m_pVframeRGB;                  //视频解码后输出帧
    AVFrame         *m_pAframe;                     //音频解码后输出帧
    struct SwsContext *m_pSwsVCtx;                  //视频帧->图片转换上下文
    AVPacket        *m_pAVpkt;                      //音视频数据包
    struct SwrContext *m_pSwsACtx;                  //音频帧->转换Qt播放格式上下文
    uint8_t         *m_pAudioBuff;                  //音频转换后使用的buf
    //step 3 init end

    QList<AVData *>   m_listData;                   //队列使用
    QSemaphore        m_sem1;                       //界面播放帧与读取frame线程同步信号量
    AVData            *m_pPreData;                  //记录当前播放视频帧数据，在播放下一帧时销毁

    static STAVControl *m_pInstance;
};

#endif //_AV_CONTROL_H_
