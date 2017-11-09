/**
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 **/

#ifndef STEREO3D_H
#define	STEREO3D_H

#include "math.h"
#include "stdio.h"

#include <compiz-core.h>
#include <compiz-plugin.h>
#include <compiz-mousepoll.h>
#include <compiz-animation.h>

#include "stereo3d_options.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#include <GL/glu.h>
#include <GL/gl.h>

static int displayPrivateIndex = 0;

typedef struct _Stereo3DWindow Stereo3DWindow;

typedef struct _Stereo3DDisplay
{
    int screenPrivateIndex;
    int windowPrivateIndex;

    MousePollFunc *mpFunc;
} Stereo3DDisplay;

enum FloatingTypeEnum { FTBACKGROUND, FTDOCK, FTWINDOW , FTNONE};


typedef struct _WndAnimationAttrs
{
        CompVector translation;
        CompVector rotation;
        float scale;
} WndAnimationAttrs;

typedef struct _AnimationManager
{
public:

    void updateWindowsPosition(CompWindow* windows, float, float);
    bool moveForegroundIn();
    bool moveForegroundOut();
    bool resetForegroundDepth();

    float getCurrentForegroundZ();

    float getCurrentMouseX();
    float getCurrentMouseY();
    void setDestMouseX(float value);
    void setDestMouseY(float value);

private:
    Point           mouseCurr;
    Point           mouseDst;
    float               foregroundCurrZ;
    float               foregroundDstZ;
    float               backgroundDepth;
    
    static void updateWindow(Stereo3DWindow * sow);
    void updateMousePosition();
} AnimationManager;

typedef struct _StereoscopicFilterBase
{
    void (*init)();
    virtual void deinit(CompScreen *s) {};
    virtual void prepareFilter(int width, int height) {};
    virtual void applyFilter(int eyenum, FragmentAttrib *fa, CompTexture *texture, CompScreen *s) {};
    virtual void cleanup() {};
} StereoscopicFilterBase;

struct InterlacedFilter : public StereoscopicFilterBase
{
        void init();
        void prepareFilter(int width, int height);
        void applyFilter(int eyenum, FragmentAttrib *fa, CompTexture *texture, CompScreen *s);
        void cleanup();

        // column or row interlaced
        bool column;
};

class AnaglyphFilter : public StereoscopicFilterBase
{
    public:
        void init();
        void deinit(CompScreen *s);
        void prepareFilter(int width, int height);
        void applyFilter(int eyenum, FragmentAttrib *fa, CompTexture *texture, CompScreen *s);
        void cleanup();

private:
        int fragmentFunctions[3];

        int getAnaglifFragmentFunction (CompTexture *texture, CompScreen *s);
};

    enum DrawingType
    {
        EyeLeft = 0,
        EyeRight,
        EyeSingle,
        Cleanup
    };

    typedef struct _CursorTexture
    {
            bool       isSet;
            GLuint     texture;
            CompScreen *screen;
            int        width;
            int        height;
            int        hotX;
            int        hotY;
    } CursorTexture;

typedef struct _Stereo3DScreen
{
    CompScreen *s;

    CursorTexture       cursorTex;
    PositionPollingHandle	pollHandle;

    AnimationManager    animationMgr;

    bool enabled;
    
    //animation
    float animPeriod;
    float  progress;
    float  time;

    bool mouseDrawingEnabled;
    int stereoType;
    
    //stereoscopic options
    float convergence;
    float parallax;

    //visual
    float edgesStrength;
    float lightingStrength;

    GLfloat projectionL [16];
    GLfloat projectionR [16];
    GLfloat projectionM [16];

    DrawingType renderingState;


    AnaglyphFilter anaglyphFilter;
    InterlacedFilter interlacedFilter;

    StereoscopicFilterBase * currFilter;


    bool
    glPaintOutput (const ScreenPaintAttrib &sAttrib,
                              const CompMatrix            &transform,
			          const Region          &region,
			          CompOutput                *output,
			          unsigned int              mask);

    PaintWindowProc paintWindow;
    PaintTransformedOutputProc paintTransformedOutput;

    void
    initProjectionMatrixChange();

    void
    setLeftEyePrjectionMatrix();

    void
    setRightEyePrjectionMatrix();

    void
    setNoConvergencePrjectionMatrix();

    void
    cleanupProjectionMatrixOperations();

    void
    updateMouseInterval (const Point &p);

    void
    freeCursor (CursorTexture * cursor);

    void
    drawCursor ();

    void
    updateCursor (CursorTexture * cursor);

    void
    enableMouseDrawing ();

    void
    disableMouseDrawing ();



    Bool
    moveForegroundIn(CompAction         *action,
		    CompActionState  state,
		    CompOption *option, int nOption);

    Bool
    moveForegroundOut(CompAction         *action,
		    CompActionState  state,
		    CompOption option, int nOption);


    Bool
    resetForegroundDepth(CompAction         *action,
		    CompActionState  state,
		    CompOption *option, int nOption);

    Bool
    toggleOn(CompAction         *action,
		    CompActionState  state,
		    CompOption *option, int nOption);

        PreparePaintScreenProc preparePaintScreen;
        DonePaintScreenProc donePaintScreen;

/********************************************************************
*******************   Window Open GL funcs    ***********************
*********************************************************************/

        PaintOutputProc paintOutput;
        DrawWindowProc drawWindow;
        DrawWindowTextureProc drawWindowTexture;

        void glDrawGeometry();

        void
        drawBackgroundWireframe(float, float);
} Stereo3DScreen;

struct _Stereo3DWindow
{
        CompWindow *window;

        WndAnimationAttrs currAttrs;
        WndAnimationAttrs dstAttrs;

        bool drawMouse;
        float opacity;
        float saturation;
        float brightness;


        FloatingTypeEnum getFloatingType();
        FloatingTypeEnum floatingType;
};

#define GET_STEREO3D_DISPLAY(d)                            \
    ((Stereo3DDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define GET_STEREO3D_SCREEN(s, sod)                         \
    ((Stereo3DScreen *) (s)->base.privates[(sod)->screenPrivateIndex].ptr)

#define GET_STEREO3D_WINDOW(w, sod)                         \
    ((Stereo3DWindow *) (w)->base.privates[(sod)->windowPrivateIndex].ptr)

#define STEREO3D_DISPLAY(d)						       \
    Stereo3DDisplay *sod = GET_STEREO3D_DISPLAY (d)

#define STEREO3D_SCREEN(s)						       \
    Stereo3DScreen *sos = GET_STEREO3D_SCREEN (s, GET_STEREO3D_DISPLAY (s->display))

#define STEREO3D_WINDOW(w)							\
    Stereo3DWindow *sow = GET_STEREO3D_WINDOW (w, GET_STEREO3D_DISPLAY (w->screen->display))

#endif
