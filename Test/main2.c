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

int main(int argc, const char * argv[]) {
    SDL_Event event;
    //Registers all available file formats and codecs with the library
    av_register_all();

    //Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    
    //Open the file
    AVFormatContext *pFormatCtx = NULL;
    
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
    int videoStream = -1;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    
    //Find the first video stream
    for(int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            videoStream = i;
            break;
        }
    }
    if(videoStream == -1)
        return -1;
    
    //Get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codec;
    AVCodec *pCodec = NULL;
    //Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec! \n");
        return -1;  //Codec not found
    }
    //Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    //Must not use the AVCodecContext from the video stream directly!
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0)
    {
        fprintf(stderr, "Couldn't copy codec context \n");
        return -1;  //Error copying codec context
    }
    //Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        return -1;  //Could not open codec
    
    
    /*
     Storing the Data
     */
    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGB = NULL;
    
    //Allocate video frame
    pFrame = av_frame_alloc();
    
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
            }
        }
        //Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
        
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                SDL_Quit();
                return -1;
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
