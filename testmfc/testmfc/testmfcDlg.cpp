
// testmfcDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "testmfc.h"
#include "testmfcDlg.h"
#include "afxdialogex.h"

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
};

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};



//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;
int thread_pause = 0;
int stop_play = 0;

int sfp_refresh_thread(void *opaque){
	thread_exit=0;
	while (!thread_exit) {
		SDL_Event event;
		event.type = SFM_REFRESH_EVENT;
		if(!thread_pause)
			SDL_PushEvent(&event);
		SDL_Delay(40);
	}
	thread_exit=0;
	//Break
	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}

UINT player(LPVOID lpParam)
{
	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame,*pFrameYUV;
	uint8_t *out_buffer;
	AVPacket *packet;
	int ret, got_picture;

	//------------SDL----------------
	int screen_w,screen_h;
	SDL_Window *screen; 
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;
	SDL_Thread *video_tid;
	SDL_Event event;

	struct SwsContext *img_convert_ctx;

	thread_pause = 0;
	stop_play    = 0;
	//char filepath[]="��˿��ʿ.mov";
	//char filepath[]="��Щ�꣬����һ��׷��Ů��.mp4";

	CtestmfcDlg *dlg = (CtestmfcDlg *)lpParam;
	char filepath[500] = {0};
	GetWindowTextA(dlg->m_url,(LPSTR)filepath,500);
		

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
		if(videoindex==-1){
			printf("Didn't find a video stream.\n");
			return -1;
		}
		pCodecCtx=pFormatCtx->streams[videoindex]->codec;
		pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
		if(pCodec==NULL){
			printf("Codec not found.\n");
			return -1;
		}
		if(avcodec_open2(pCodecCtx, pCodec,NULL)<0){
			printf("Could not open codec.\n");
			return -1;
		}
		pFrame=av_frame_alloc();
		pFrameYUV=av_frame_alloc();
		out_buffer=(uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
		avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

		img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
			pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 


		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {  
			printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
			return -1;
		} 
		//SDL 2.0 Support for multiple windows
		screen_w = pCodecCtx->width;
		screen_h = pCodecCtx->height;
		//screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		//	screen_w, screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
		screen = SDL_CreateWindowFrom(dlg->GetDlgItem(IDC_SCREEN)->GetSafeHwnd());
		if(!screen) {  
			printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
			return -1;
		}
		sdlRenderer = SDL_CreateRenderer(screen, -1, 0);  
		//IYUV: Y + U + V  (3 planes)
		//YV12: Y + V + U  (3 planes)
		sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pCodecCtx->width,pCodecCtx->height);  

		sdlRect.x=0;
		sdlRect.y=0;
		sdlRect.w=screen_w;
		sdlRect.h=screen_h;

		packet=(AVPacket *)av_malloc(sizeof(AVPacket));

		video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);
		//------------SDL End------------
		//Event Loop

		for (;;) {
			//Wait
			SDL_WaitEvent(&event);
			if(event.type==SFM_REFRESH_EVENT){
				//------------------------------
				while(1)
				{
					if(av_read_frame(pFormatCtx, packet)>=0)
					{
						if(packet->stream_index==videoindex){
							ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
							if(ret < 0){
								printf("Decode Error.\n");
								return -1;
							}
							if(got_picture){
								sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
								//SDL---------------------------
								SDL_UpdateTexture( sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );  
								SDL_RenderClear( sdlRenderer );  
								//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );  
								SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, NULL);  
								SDL_RenderPresent( sdlRenderer );  
								//SDL End-----------------------
							}
							break;
						}
						else
						{
							av_free_packet(packet);
							continue;
						}
						av_free_packet(packet);					
					}else{
						//Exit Thread
						thread_exit=1;
						break;
					}
				}
			}
			else if(event.type==SDL_QUIT)
			{
				thread_exit=1;
			}
			else if(event.type==SFM_BREAK_EVENT)
			{
				break;
			}
			else if(event.type==SDL_WINDOWEVENT)
			{
				//If Resize
				SDL_GetWindowSize(screen,&screen_w,&screen_h);
			}

			if(stop_play)
			{
				thread_exit=1;
				break;
			}

		}

		sws_freeContext(img_convert_ctx);

		SDL_Quit();
		dlg->GetDlgItem(IDC_SCREEN)->ShowWindow(SW_SHOWNORMAL);
		//--------------
		av_frame_free(&pFrameYUV);
		av_frame_free(&pFrame);
		avcodec_close(pCodecCtx);
		avformat_close_input(&pFormatCtx);
		return 0;
}

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CtestmfcDlg �Ի���




CtestmfcDlg::CtestmfcDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CtestmfcDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CtestmfcDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_URL, m_url);
}

BEGIN_MESSAGE_MAP(CtestmfcDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_PLAY, &CtestmfcDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_PAUSE, &CtestmfcDlg::OnBnClickedPause)
	ON_BN_CLICKED(IDC_STOP, &CtestmfcDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_ABOUT, &CtestmfcDlg::OnBnClickedAbout)
	ON_BN_CLICKED(CLOSE, &CtestmfcDlg::OnBnClickedClose)
	ON_BN_CLICKED(IDC_FILE, &CtestmfcDlg::OnBnClickedFile)
	ON_EN_CHANGE(IDC_URL, &CtestmfcDlg::OnEnChangeUrl)
END_MESSAGE_MAP()


// CtestmfcDlg ��Ϣ�������

BOOL CtestmfcDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CtestmfcDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CtestmfcDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CtestmfcDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CtestmfcDlg::OnBnClickedPlay()
{
	CString str;
	m_url.GetWindowText(str);
	if(str.IsEmpty())
	{
		AfxMessageBox("�������ļ�·��");
        return;
	}
	AfxBeginThread(player,this);
}


void CtestmfcDlg::OnBnClickedPause()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	thread_pause = !thread_pause;
}


void CtestmfcDlg::OnBnClickedStop()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	stop_play = 1;
}


void CtestmfcDlg::OnBnClickedAbout()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	CAboutDlg dlg1;
	dlg1.DoModal(); // ���������ȥ�����ط�
}


void CtestmfcDlg::OnBnClickedClose()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
}


void CtestmfcDlg::OnBnClickedFile()
{
	//CString str1;
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	//m_url.GetWindowText(str1);
	//str1.Format("%s",avcodec_configuration());	
	//AfxMessageBox(str1);
	//player();
	CString FilePathName;
	CFileDialog dlg(TRUE,NULL,NULL,NULL);
	if(dlg.DoModal()==IDOK)
	{
		FilePathName=dlg.GetPathName();
		m_url.SetWindowTextA(FilePathName);
	}
	
}


void CtestmfcDlg::OnEnChangeUrl()
{
	// TODO:  ����ÿؼ��� RICHEDIT �ؼ���������
	// ���ʹ�֪ͨ��������д CDialogEx::OnInitDialog()
	// ���������� CRichEditCtrl().SetEventMask()��
	// ͬʱ�� ENM_CHANGE ��־�������㵽�����С�

	// TODO:  �ڴ���ӿؼ�֪ͨ����������
}
