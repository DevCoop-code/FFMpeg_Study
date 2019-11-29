//
//  main.cpp
//  ffMpeg_Tutorial
//
//  Created by HanGyo Jeong on 2019/11/27.
//  Copyright © 2019 HanGyoJeong. All rights reserved.
//

#include <SDL2/SDL.h>
#include <stdio.h>
#include <iostream>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int y;
    
    //Open file
    sprintf(szFilename, "frame%d.ppm", iFrame);
    pFile = fopen(szFilename, "wb");
    if(pFile == NULL)
        return;
    
    //Write header
    fprintf(pFile, "P6\n%d %d\n255\n", width, height);
    
    //Write pixel data
    for(int y=0; y<height; y++)
    {
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    }
    
    //Close file
    fclose(pFile);
}

int main(int argc, const char * argv[]) {
    //Registers all available file formats and codecs with the library
    av_register_all();

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
    //Allocate an AVFrame structure
    pFrameRGB = av_frame_alloc();
    if(pFrameRGB == NULL)
        return -1;
    //Even though we've allocated the frame, still need a place to put the raw data when convert it
    uint8_t *buffer = NULL;
    int numBytes;
    //Determine required buffer size and allocate buffer
    numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    
    /*
     Assign appropriate parts of buffer to image planes in pFrameRGB
    */
     //Note that pFrameRGB is an AVFrame but AVFrame is a superset of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    
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
                //Convert the image from its native format(pCodecCtx->pix_fmt) to RGB
                sws_scale(sws_ctx,
                          (uint8_t const * const *)pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecCtx->height,
                          pFrameRGB->data,
                          pFrameRGB->linesize);
                
                //Save the frame to disk
                if(++i <= 5)
                {
                    SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
                }
            }
        }
        
        //Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }
    return 0;
}
