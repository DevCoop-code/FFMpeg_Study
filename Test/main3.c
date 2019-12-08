//
//  main.cpp
//  ffMpeg_Tutorial
//
//  Created by HanGyo Jeong on 2019/11/27.
//  Copyright © 2019 HanGyoJeong. All rights reserved.
//
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include <stdio.h>
#include <assert.h>
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

int quit = 0;   //To Quit the Program

typedef struct PacketQueue
{
    AVPacketList *first_pkt, *last_pkt;     //Linked list for packets
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;
PacketQueue audioq;

void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;
    if(av_dup_packet(pkt) < 0)
    {
        return -1;
    }
    
    pkt1 = av_malloc(sizeof(AVPacketList));
    if(!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    
    SDL_LockMutex(q->mutex);
    
    if(!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    
    SDL_CondSignal(q->cond);    //send a signal to get function(if it is waiting)
    SDL_UnlockMutex(q->mutex);
    
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;
    
    SDL_LockMutex(q->mutex);
    
    for(;;)
    {
        if(quit)
        {
            ret = -1;
            break;
        }
        
        pkt1 = q->first_pkt;
        if(pkt1)
        {
            q->first_pkt = pkt1->next;
            if(!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        }
        else if(!block)
        {
            ret = 0;
            break;
        }
        else
        {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size)
{
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;
    
    int len1, data_size = 0;
    
    for(;;)
    {
        while (audio_pkt_size > 0)
        {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            if(len1 < 0)
            {
                //If error, skip frame
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            if(got_frame)
            {
                data_size = av_samples_get_buffer_size(NULL,
                                                       aCodecCtx->channels,
                                                       frame.nb_samples,
                                                       aCodecCtx->sample_fmt,
                                                       1);
                //assert(data_size <= buf_size);
                memcpy(audio_buf, frame.data[0], data_size);
            }
            if(data_size <= 0)
            {
                //No data yet, get more frames
                continue;
            }
            //We have data, return it and come back for more later
            return data_size;
        }

        fprintf(stderr, "hankyo1 \n");
        // if(pkt.data)
        // {
        //     fprintf(stderr, "hankyo3 \n");
        //     av_free_packet(&pkt);
        // }
        
        if(quit)
        {
            fprintf(stderr, "hankyo2 \n");
            return -1;
        }

        fprintf(stderr, "hankyo4 \n");
        
        if(packet_queue_get(&audioq, &pkt, 1) < 0)
        {
            fprintf(stderr, "hankyo3 \n");
            return -1;
        }
        
        fprintf(stderr, "hankyo5 \n");

        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;

        fprintf(stderr, "audio packet size: %d \n", audio_pkt_size);
    }
}

/*
 [Fetching Packets]
 userdata: pointer which gave to SDL
 stream: buffer which we will be writing audio data
 len: size of that buffer
 */
void audio_callback(void *userdata, Uint8 *stream, int len)
{
    fprintf(stderr, "audio callback called \n");

    AVCodecContext *aCodecCtx = (AVCodecContext*)userdata;
    int len1, audio_size;
    
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;
    
    // fprintf(stderr, "audio size of buffer : %d \n", len);
    while (len > 0)
    {
        // fprintf(stderr, "audio buf index(%d), audio buf size(%d) \n", audio_buf_index, audio_buf_size);
        if(audio_buf_index >= audio_buf_size)
        {
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            
            // fprintf(stderr, "audio size : %d \n", audio_size);
            
            if(audio_size < 0)
            {
                //if error, output silence
                audio_buf_size = 1024;
                memset(audio_buf, 0, audio_buf_size);
            }
            else
            {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        
        if(len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int main(int argc, const char * argv[]) {
    AVFormatContext *pFormatCtx = NULL;
    int videoStream, audioStream;

    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodecContext *aCodecCtxOrig = NULL;
    AVCodecContext *aCodecCtx = NULL;
    
    SDL_AudioSpec wanted_spec, spec;
    
    AVCodec *pCodec = NULL;
    AVCodec *aCodec = NULL;
    
    /*
     Storing the Data
     */
    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGB = NULL;
    
    SDL_Event event;

    //Registers all available file formats and codecs with the library
    av_register_all();

    //Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    
    /*
     Read File Header
     */
    //Reads file header and Stores information about the file format in the AVFormatContext
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    {
        return -1;  //Couldn't open file
    }
    
    /*
     Check out the stream information
     */
    //Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        return -1;  //Couldn't find stream information
    }
    
    /*
     Dump information about file onto standard error
     */
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    
    //pFormatCtx->streams :Array of pointters
    //pFormatCtx->nb_streams :Number of elements in AVFormatContext.streams
    
    //Find the first video stream
    videoStream = -1 ;
    audioStream = -1;
    for(int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0)
        {
            videoStream = i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
        {
            audioStream = i;
        }
    }
    if(videoStream == -1)
        return -1;
    if(audioStream == -1)
        return -1;

    //Get a pointer to the codec context for the video stream
    aCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;
    
    //Find the decoder for the video stream
    aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);

    if(aCodec == NULL)
    {
        fprintf(stderr, "Unsupported Audio codec! \n");
        return -1;  //Codec not found
    }
    //Copy context
    aCodecCtx = avcodec_alloc_context3(aCodec);
    //Must not use the AVCodecContext from the video stream directly!
    if((avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0))
    {
        fprintf(stderr, "Couldn't copy Audio codec context \n");
        return -1;  //Error copying codec context
    }
    /*
     Contained within the codec context is all the information we need to set up our audio
     
     freq: Sample Rate
     format: SDL what format we will be giving it
        ex] S16SYS : S-> signed / 16-> each sample is 16 bits / SYS-> endian-order
     channels: Number of audio channels
     silence: value that indicated silence
     samples: size of the audio buffer
     */
    wanted_spec.freq = aCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = aCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = aCodecCtx;
    
    if(SDL_OpenAudio(&wanted_spec, &spec) < 0)
    {
        fprintf(stderr, "SDL OpenAudio: %s \n", SDL_GetError());
        return -1;
    }
    
    //Open codec
    if((avcodec_open2(aCodecCtx, aCodec, NULL) < 0))
    {
        fprintf(stderr, "Could not open audio codec \n");
        return -1;  //Could not open codec
    }
    
    packet_queue_init(&audioq);
    SDL_PauseAudio(0);  //To starts the audio device
    
    //Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;

    //Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL)
    {
        fprintf(stderr, "Unsupported video codec! \n");
        return -1;  //Codec not found
    }

    //Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    //Must not use the AVCodecContext from the video stream directly!
    if( (avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) )
    {
        fprintf(stderr, "Couldn't copy video codec context \n");
        return -1;  //Error copying codec context
    }

    //Open codec
    if((avcodec_open2(pCodecCtx, pCodec, NULL) < 0))
    {
        fprintf(stderr, "Could not open video codec \n");
        return -1;  //Error copying codec context
    }


    //Allocate video frame
    pFrame = av_frame_alloc();
    
    //Make a screen to put video
    // #ifndef __DARWIN__
    //     screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
    // #else
    //     screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
    // #endif

    //Set up a screen with the given width * height
    SDL_Window *screen;
    screen = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width, pCodecCtx->height, SDL_WINDOW_OPENGL);
    if(!screen)
    {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        return -1;
    }
    
    //Allocate a place to put YUV image on that screen
    SDL_Renderer *renderer = SDL_CreateRenderer(screen, -1, 0);
    if(!renderer)
    {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        return -1;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                            SDL_PIXELFORMAT_YV12,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            pCodecCtx->width,
                                            pCodecCtx->height);
    if(!texture)
    {
        fprintf(stderr, "SDL: could not create texture - exiting\n");
        return -1;
    }
    
    /*
     Read through the entire video stream by reading in the packet, decoding it into our frame and once our frame is complete, will convert and save it
     */
    struct SwsContext *sws_ctx = NULL;
    int frameFinished;
    
    AVPacket packet;
    //Initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_RGB24,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL);
    
    //Set up YV12 pixel array(12 bits per pixel)
    Uint8 *yPlane, *uPlane, *vPlane;
    size_t yPlaneSz, uvPlaneSz;
    int uvPitch;
    
    yPlaneSz = pCodecCtx->width * pCodecCtx->height;
    uvPlaneSz = pCodecCtx->width * pCodecCtx->height / 4;
    yPlane = (Uint8*)malloc(yPlaneSz);
    uPlane = (Uint8*)malloc(uvPlaneSz);
    vPlane = (Uint8*)malloc(uvPlaneSz);
    if(!yPlane || !uPlane || !vPlane)
    {
        fprintf(stderr, "Could not allocate pixel buffers - exiting\n");
        exit(1);
    }
    
    uvPitch = pCodecCtx->width / 2; //Pitch is the term SDL uses to refer to the width of a fiven line of data
    
    int i=0;
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        //Is this a packet from the video stream?
        if(packet.stream_index == videoStream)
        {
            //Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            
            //Did we get a video frame?
            if(frameFinished)
            {
                AVPicture pict;
                pict.data[0] = yPlane;
                pict.data[1] = uPlane;
                pict.data[2] = vPlane;
                pict.linesize[0] = pCodecCtx->width;    //y width
                pict.linesize[1] = uvPitch;             //u width
                pict.linesize[2] = uvPitch;             //v width
                
                //Convert the image into YUV format that SDL uses
                sws_scale(sws_ctx,
                          (uint8_t const* const*)pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecCtx->height,
                          pict.data,
                          pict.linesize);
                
                SDL_UpdateYUVTexture(texture,
                                     NULL,
                                     yPlane,
                                     pCodecCtx->width,
                                     uPlane,
                                     uvPitch,
                                     vPlane,
                                     uvPitch);
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);
                
                av_free_packet(&packet);
            }
        }
        else if(packet.stream_index == audioStream)
        {
            packet_queue_put(&audioq, &packet);
        }
        else
        {
            av_free_packet(&packet);
        }
        
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                quit = 1;
                SDL_Quit();
                exit(0);
                break;
                
            default:
                break;
        }
    }
    
    //Free the yuv frame
    av_frame_free(&pFrame);
    free(yPlane);
    free(uPlane);
    free(vPlane);
    
    //Close the codec
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    
    //Close the video file
    avformat_close_input(&pFormatCtx);
    
    return 0;
}