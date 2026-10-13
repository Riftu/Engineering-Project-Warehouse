
#ifndef _COMMON_H_
#define _COMMON_H_

#include <QImage>
#include <QTime>

#define SOFT_VERSION            "v_1.0"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
}

#define MAX_AUDIO_FRAME_SIZE	192000

#define LOG_FILE_ROW_MAX        50000
#define LOG_FILE_MAX            10

#define DEBUG                   1

//time, use to log
#define _TIME_ qPrintable(QTime::currentTime().toString("hh:mm:ss:zzz"))
#define SECURE_DELETE(_X_) if (_X_) {qDebug("%s : %s: %d SECURE_DELETE"#_X_, __FILE__,__FUNCTION__,__LINE__); delete (_X_); (_X_) = NULL;}

#ifdef EIM_ENV_PUBLISH
#define PRINT_LOG
#endif

#define FUNCTION_CALL_LOG

#ifdef FUNCTION_CALL_LOG
#define FUNCTION_ENTER qDebug( "%s %s %d %s start!",__FILE__,__FUNCTION__,__LINE__,_TIME_ );
#define FUNCTION_EXIT  qDebug( "%s %s %d %s end!",__FILE__,__FUNCTION__,__LINE__,_TIME_ );

#else
#define FUNCTION_ENTER ;
#define FUNCTION_EXIT ;

#endif

#define AES_KEY         "2e0a3ac2f9f6be79c217f1bf21bae23a"
#define AV_KEY          "qqqq"

#define USER_DOMAIN "http://47.120.61.156/"
#define USER_LOGIN_URL "home/MemberAction/MemberLogin"
#define USER_GETAVLIST_URL  "home/MemberAction/MemberVideoList"
#define USER_RESETPWD "home/MemberAction/MemberResetPwd"

#define USER_DATA_PATH QDir::homePath()+ "/vplayer"

enum AV_SIGNAL_TYPE{
    //0-99 av net
    AV_NET_LOGIN_SUCCESS = 0,           //登录成功
    AV_NET_LOGIN_ERROR,                 //未登录
    AV_NET_AVLIST,                      //登录后获取播放列表
    AV_NET_PWDRESET,

    //100-199 controlbar
    CONTROL_BAR_PLAY = 100,
    CONTROL_BAR_SHOW_SETUP,
    CONTROL_BAR_NEXT_PLAY,
    CONTROL_BAR_PRE_PLAY,
    CONTROL_BAR_PLAY_END,
    //200-299 sidebar
    SIDEBAR_SHOW_HIDE = 200,
    SIDEBAR_DOUBLE_ITEM,
    SIDEBAR_REFRESH,
    //300-309 titlebar
    TITLE_BAR_SHOW_LOGIN = 300,
};

enum AV_TYPE_ERR {
    //0-99 av_control
    CTL_OK = 0,
    CTL_INIT_ERR,                   //初始化错误
    CTL_OPEN_ERR,                   //打开文件错误
    CTL_FILENAME_ERR,               //文件检测错误 mp4 读写权限
    CTL_STREAM_INFO_ERR,            //打开文件后获取流信息错误
    CTL_INIT_1_ERR,
    CTL_INIT_2_ERR,
    CTL_INIT_3_ERR,
    //100-199  audio thread
    AUD_OK = 100,
    AUD_ERROR,
    //200-299 net work
    AV_NET_INIT = 200,
    AV_NET_REQUESTING,              //正在进行网络请求
    AV_NET_FREE,                    //空闲状态

    //300-399 log
    LOG_ERR = 300,
    LOG_OK,
};

enum AV_TYPE_DATA
{
    AV_TYPE_NONE = -1,
    AV_TYPE_VIDEO = 0,
    AV_TYPE_AUDIO
};

class AVData
{
public:
    AVData()
    {
        m_iPlayTime = 0;
        m_iNextTime = 0;
        m_iType = AV_TYPE_NONE;
        m_image = NULL;
    }
    ~AVData()
    {
        m_iPlayTime = 0;
        m_iNextTime = 0;
        m_iType = AV_TYPE_NONE;
        if (NULL != m_image)
        {delete m_image; m_image = NULL;}
    }

    unsigned int    m_iPlayTime;        //播放时间
    unsigned int    m_iNextTime;        //下一帧时间
    int             m_iType;            //数据类型
    QImage          *m_image;           //图片buf

};

class AvFileInfo
{
public:
    AvFileInfo()
    {
        m_strUrl = "";
        m_iVDurationTs = 0;
        m_iVFrames = 0;
        m_iADurationTs = 0;
        m_iAFrames = 0;
    }

    AvFileInfo(const AvFileInfo &obj)
    {
        m_strUrl = obj.m_strUrl;
        m_iVDurationTs = obj.m_iVDurationTs;
        m_iVFrames = obj.m_iVFrames;
        m_iADurationTs = obj.m_iADurationTs;
        m_iAFrames = obj.m_iAFrames;
    }

    ~AvFileInfo()
    {
        m_strUrl = "";
        m_iVDurationTs = 0;
        m_iVFrames = 0;
        m_iADurationTs = 0;
        m_iAFrames = 0;
    }

public:
    QString m_strUrl;
    int     m_iVDurationTs;
    int     m_iVFrames;
    int     m_iADurationTs;
    int     m_iAFrames;
};

class AVLoginStatus
{
public:
    AVLoginStatus()
    {
        m_iStatus = -1;
        m_strUser = "";
    }
    ~AVLoginStatus()
    {
        m_iStatus = -1;
        m_strUser = "";
    }

public:
    int m_iStatus;
    QString m_strUser;
};

#define CAPTURE_PROCESS                         "检测到录屏程序，确定后程序自动退出!"
#define CAPTURE_PROCESS_TITLE                   "程序退出提示"
#define USER_LOGIN_ERROR                             "登录失败，请重新登录！"
#define USER_LOGIN_ERROR_TITLE                       "登录失败"
#define AV_FILE_NOT_DOWNLOAD                    "未下载"
#define AV_FILE_DOWNLOAD                        "已下载"

#define USER_LOGIN_NAME_PWD_NULL_ERROR          "用户名或密码为空"
#define USER_LOGIN_NAME_PWD_ERROR_TITLE         "用户名或密码错误"
#define USER_LOGIN_NAME_RESETPWD_NULL_ERROR     "新密码与旧密码一致"
#define USER_LOGIN_NAME_RESETPWD_ERROR          "重置密码失败"
#define USER_LOGIN_NAME_RESETPWD_ERROR_TITLE    "重置密码"
#define SIDEBAR_PLAY_LIST                       "  播放列表"
#define USER_NOLOGIN                            "未登录"
#define PLAYER_TITLE                            "  Vision player"

Q_DECLARE_METATYPE(AvFileInfo)
Q_DECLARE_METATYPE(AVLoginStatus)

#endif //_COMMON_H_
