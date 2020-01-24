// frames.h
#ifndef FRAMES_H
#define FRAMES_H

#define NSIZE 1024

class Frames
{
public:
    int mode;
    int frames;
    int recvbuflen, size;
    int start;

    Frames(int inframes)
    {
        tomode = mode = 0;
        toframes = frames = inframes;
        size = NSIZE / frames;
        recvbuflen = 6 * size;
        start = 0;
    }
    int queue_mode(int inmode)
    {
        tomode = inmode;
        return tomode;
    }
    int queue_frames_up()
    {
        if (toframes < NSIZE)
            toframes *= 2;
        return toframes;
    }
    int queue_frames_down()
    {
        if (toframes > 1)
            toframes /= 2;
        return toframes;
    }
    int frame_complete(int *n, int *fstart)
    {
        int complete = 0;
        if (*n  == start + size)
        {
            if (mode == 0 || start == 0)
            {
                *fstart = start;
                complete = 1;
            }
            start += size;
            if (*n == NSIZE)
            {
                *n = 0;
                start = 0;
                push_queued();
            }
        }
        return complete;
    }
    int push_queued()
    {
        if (tomode != mode)
        {
            mode = tomode;
        }
        if (toframes != frames)
        {
            frames = toframes;
            size = NSIZE / frames;
            recvbuflen = 6 * size;
        }
        return 0;
    }

private:
    int toframes, tomode = 0;
};

#endif