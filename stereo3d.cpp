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

#include "stereo3d.h"

#define PI 3.1415926f

static void
enableMouseDrawing(CompScreen *s);
static void
disableMouseDrawing(Stereo3DScreen *sos);

static void
frustum (GLfloat *m,
	 GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
    GLfloat x, y, a, b, c, d;

    x = (2.0 * nearval) / (right - left);
    y = (2.0 * nearval) / (top - bottom);
    a = (right + left) / (right - left);
    b = (top + bottom) / (top - bottom);
    c = -(farval + nearval) / ( farval - nearval);
    d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col * 4 + row]
    M(0,0) = x;     M(0,1) = 0.0f;  M(0,2) = a;      M(0,3) = 0.0f;
    M(1,0) = 0.0f;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0f;
    M(2,0) = 0.0f;  M(2,1) = 0.0f;  M(2,2) = c;      M(2,3) = d;
    M(3,0) = 0.0f;  M(3,1) = 0.0f;  M(3,2) = -1.0f;  M(3,3) = 0.0f;
#undef M

}

static void
perspective (GLfloat *m,
	     GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar,
	     GLfloat xShift)
{
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * tan (fovy * M_PI / 360.0);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    frustum (m, xmin + xShift, xmax + xShift, ymin, ymax, zNear, zFar);
}

static void
stereo3dPreparePaintScreen (CompScreen *s,
			    int        ms)
{
    STEREO3D_SCREEN (s);

    UNWRAP (sos, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, ms);
    WRAP (sos, s, preparePaintScreen, stereo3dPreparePaintScreen);

    if(!sos->enabled)
        return;

    if(stereo3dGetDrawmouse (s->display))
    {
        if(!sos->mouseDrawingEnabled)
        {
            sos->mouseDrawingEnabled = true;
            enableMouseDrawing(s);
        }
    }
    else
    {
        if(sos->mouseDrawingEnabled)
        {
            sos->mouseDrawingEnabled = false;
            disableMouseDrawing(sos);
        } 
    }

    sos->stereoType = stereo3dGetOutputMode(s->display);
    switch(sos->stereoType)
    {
        case 0:
            //2.5D, do nothing...
            sos->currFilter = &sos->anaglyphFilter;
            break;

        case 1:
            sos->currFilter = &sos->anaglyphFilter;
            break;

        case 2:
            sos->interlacedFilter.column = false;
            sos->currFilter = &sos->interlacedFilter;
            break;

        case 3:
            sos->interlacedFilter.column = true;
            sos->currFilter = &sos->interlacedFilter;
            break;
    }

    // distance of near plane
    float nearval = 0.1f;
    // distance of far plane
    float farval = 100.0f;
    // aspect ratio
    float aspect = 1.0f;

    float fov = stereo3dGetFov(s->display);

    // strength of stereo efect, maximum disparity in px
    float maxDisparityInPx = stereo3dGetStrength(s->display);

    // screen width
    float screenWidthPx = s->width;


    float tanfov = 0.5f / tan(fov * M_PI / 360.0);

    // stereo attributes                                0.1    0.577..
    sos->convergence = (maxDisparityInPx / screenWidthPx) * (nearval/tanfov);
    sos->parallax = (maxDisparityInPx / screenWidthPx);

    if(sos->stereoType != 0)
    {
        //left eye projection matrix
        perspective (sos->projectionL, fov, aspect, nearval, farval, -sos->convergence);

        //right eye projection matrix
        perspective (sos->projectionR, fov, aspect, nearval, farval, sos->convergence);
    }
    else
    {
        //zero convergence for 2.5d effect
        perspective (sos->projectionM, fov, aspect, nearval, farval, 0.0f);        
    }


    float depth = stereo3dGetDepth(s->display);
    sos->lightingStrength = stereo3dGetLightingStrength(s->display);
    sos->edgesStrength = stereo3dGetEdgesStrength(s->display);

    sos->animationMgr.updateWindowsPosition(s->windows, depth, sos->lightingStrength);
}

static Bool
stereo3dPaintOutput (CompScreen              *s,
		     const ScreenPaintAttrib *sa,
		     const CompTransform     *origTransform,
		     Region                  region,
		     CompOutput              *output,
		     unsigned int            mask)
{
    Bool status;
    CompTransform *mTransform;

    STEREO3D_SCREEN (s);

    mTransform = (CompTransform*)memcpy (malloc (sizeof (CompMatrix)), origTransform, sizeof (CompMatrix));

    if(sos->enabled)
    {
        mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

        mask |= PAINT_SCREEN_TRANSFORMED_MASK | PAINT_SCREEN_CLEAR_MASK;
    }

    UNWRAP (sos, s, paintOutput);
    status = (*s->paintOutput) (s, sa, mTransform, region, output, mask);
    WRAP (sos, s, paintOutput, stereo3dPaintOutput);

    free (mTransform);

    return status;
}

static void
stereo3dPaintTransformedOutput (CompScreen              *s,
			        const ScreenPaintAttrib *sa,
			        const CompTransform     *origTransform,
			        Region                  region,
			        CompOutput              *output,
			        unsigned int            mask)
{
    CompTransform *mTransform;

    STEREO3D_SCREEN (s);

    mTransform = (CompTransform*)memcpy (malloc (sizeof (CompMatrix)), origTransform, sizeof (CompMatrix));

    if(sos->enabled)
    {
        mask |= PAINT_SCREEN_CLEAR_MASK;
        mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

        sos->currFilter->prepareFilter(s->width, s->height);

        UNWRAP (sos, s, paintTransformedOutput);
        (*s->paintTransformedOutput) (s, sa, mTransform, region, output, mask);
        WRAP (sos, s, paintTransformedOutput, stereo3dPaintTransformedOutput);

        sos->currFilter->cleanup();
    }
    else
    {
        UNWRAP (sos, s, paintTransformedOutput);
        (*s->paintTransformedOutput) (s, sa, mTransform, region, output, mask);
        WRAP (sos, s, paintTransformedOutput, stereo3dPaintTransformedOutput);
    }
}

static void
stereo3dDonePaintScreen (CompScreen *s)
{
    STEREO3D_SCREEN (s);

    if(sos->enabled)
    {
        //FIXME: probably I don't need do damage all screen
        damageScreen (s);
    }

    UNWRAP (sos, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (sos, s, donePaintScreen, stereo3dDonePaintScreen);
}

/* based on drawing cursor in ezoom */
static void
freeCursor (Stereo3DScreen *sos, CursorTexture * cursor)
{
    if (cursor==NULL)
        return ;

    if (!cursor->isSet)
	return;

    cursor->isSet = FALSE;

    if(cursor != NULL && cursor->texture != 0)
        glDeleteTextures (1, &cursor->texture);
    
    cursor->texture = 0;
}

static void
drawCursor (Stereo3DScreen *sos)
{
    if (sos->cursorTex.isSet && sos->mouseDrawingEnabled)
    {
	CompTransform      sTransform;// = transform;
	int           x, y;


        matrixTranslate (&sTransform, 0.0f, 0.0f, sos->animationMgr.getCurrentForegroundZ ());

	transformToScreenSpace (sos->s, &sos->s->outputDev[sos->s->currentOutputDev], -DEFAULT_Z_CAMERA, &sTransform);

        glPushMatrix ();
	glLoadMatrixf (sTransform.m);
	glTranslatef ( sos->animationMgr.getCurrentMouseX (),
                   sos->animationMgr.getCurrentMouseY (),
                   0.0f);

        
	x = -sos->cursorTex.hotX;
	y = -sos->cursorTex.hotY;

	glEnable (GL_BLEND);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, sos->cursorTex.texture);
	glEnable (GL_TEXTURE_RECTANGLE_ARB);


	glBegin (GL_QUADS);
	glTexCoord2d (0, 0);
	glVertex2f (x, y);
	glTexCoord2d (0, sos->cursorTex.height);
	glVertex2f (x, y + sos->cursorTex.height);
	glTexCoord2d (sos->cursorTex.width, sos->cursorTex.height);
	glVertex2f (x + sos->cursorTex.width, y + sos->cursorTex.height);
	glTexCoord2d (sos->cursorTex.width, 0);
	glVertex2f (x + sos->cursorTex.width, y);
	glEnd ();

	glDisable (GL_BLEND);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, 0);
	glDisable (GL_TEXTURE_RECTANGLE_ARB);
	glPopMatrix ();
    }
}

/* Create (if necessary) a texture to store the cursor,
 * fetch the cursor with XFixes. Store it.  */
static void
updateCursor (CompScreen *s)
{
//    compLogMessage ("stereo3d", CompLogLevelWarn, "updateCursor!");
    unsigned char *pixels;
    int           i;
    Display       *dpy = s->display->display;

    STEREO3D_SCREEN (s);

    if (!sos->cursorTex.isSet)
    {
	sos->cursorTex.isSet = true;
	sos->cursorTex.screen = sos->s;
	glEnable (GL_TEXTURE_RECTANGLE_ARB);
	glGenTextures (1, &sos->cursorTex.texture);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, sos->cursorTex.texture);

	glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
			 GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
			 GL_TEXTURE_WRAP_T, GL_CLAMP);
    } else {
	glEnable (GL_TEXTURE_RECTANGLE_ARB);
    }

    XFixesCursorImage *ci = XFixesGetCursorImage (dpy);

    if (ci)
    {
	sos->cursorTex.width = ci->width;
	sos->cursorTex.height = ci->height;
	sos->cursorTex.hotX = ci->xhot;
	sos->cursorTex.hotY = ci->yhot;
	pixels = (unsigned char *) malloc (ci->width * ci->height * 4);

	if (!pixels)
	{
	    XFree (ci);
            
	    return;
	}

	for (i = 0; i < ci->width * ci->height; i++)
	{
	    unsigned long pix = ci->pixels[i];
	    pixels[i * 4] = pix & 0xff;
	    pixels[(i * 4) + 1] = (pix >> 8) & 0xff;
	    pixels[(i * 4) + 2] = (pix >> 16) & 0xff;
	    pixels[(i * 4) + 3] = (pix >> 24) & 0xff;
	}

	XFree (ci);
    }
    else
    {
	/* Fallback R: 255 G: 255 B: 255 A: 255
	 * FIXME: Draw a cairo mouse cursor */

	sos->cursorTex.width = 1;
	sos->cursorTex.height = 1;
	sos->cursorTex.hotX = 0;
	sos->cursorTex.hotY = 0;
	pixels = (unsigned char *) malloc (sos->cursorTex.width * sos->cursorTex.height * 4);

	if (!pixels)
	    return;

	for (i = 0; i < sos->cursorTex.width * sos->cursorTex.height; i++)
	{
	    unsigned long pix = 0x00ffffff;
	    pixels[i * 4] = pix & 0xff;
	    pixels[(i * 4) + 1] = (pix >> 8) & 0xff;
	    pixels[(i * 4) + 2] = (pix >> 16) & 0xff;
	    pixels[(i * 4) + 3] = (pix >> 24) & 0xff;
	}

	compLogMessage ("stereo3d", CompLogLevelWarn, "unable to get system cursor image!");
    }

    glBindTexture (GL_TEXTURE_RECTANGLE_ARB, sos->cursorTex.texture);
    glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, sos->cursorTex.width,
		  sos->cursorTex.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture (GL_TEXTURE_RECTANGLE_ARB, 0);
    glDisable (GL_TEXTURE_RECTANGLE_ARB);

    free (pixels);
}



/********************************************************************
*******************   Window Open GL funcs    ***********************
*********************************************************************/

static Bool
stereo3dPaintWindow (CompWindow              *w,
		     const WindowPaintAttrib *attrib,
		     const CompTransform     *transform,
		     Region                  region,
		     unsigned int            mask)
{
    CompTransform *mTransform;
    WindowPaintAttrib *mAttrib;

    STEREO3D_SCREEN(w->screen);
    STEREO3D_WINDOW(w);

    mTransform = (CompTransform*)memcpy (malloc (sizeof (CompTransform)), transform, sizeof (CompTransform));
    mAttrib = (WindowPaintAttrib*)memcpy (malloc (sizeof (WindowPaintAttrib)), attrib, sizeof (WindowPaintAttrib));

    if(sos->enabled)
    {
        mask |= PAINT_WINDOW_TRANSFORMED_MASK;
        mask |= PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;

        // transform the window to its position
        matrixTranslate (mTransform, w->width/2.0f, w->height/2.0f, 0.0f);
        matrixScale (mTransform, sow->currAttrs.scale, sow->currAttrs.scale, 1.0f);
        matrixRotate (mTransform, sow->currAttrs.rotation.y, 0.0f, 0.1f, 0.0f);
        matrixRotate (mTransform, sow->currAttrs.rotation.x, 0.1f, 0.0f, 0.0f);
        matrixTranslate (mTransform, -w->width/2.0f, -w->height/2.0f, 0.0f);
        matrixTranslate (mTransform, sow->currAttrs.translation.x, sow->currAttrs.translation.y, sow->currAttrs.translation.z);

        mAttrib->opacity *= sow->opacity;
        mAttrib->brightness *= sow->brightness;
        mAttrib->saturation *= sow->saturation;
    }

    UNWRAP (sos, w->screen, paintWindow);
    Bool status = (*w->screen->paintWindow) (w, mAttrib, mTransform, region, mask);
    WRAP (sos, w->screen, paintWindow, stereo3dPaintWindow);

    free (mAttrib);
    free (mTransform);

    return status;
}

static void
initProjectionMatrixChange();

static void
setLeftEyeProjectionMatrix (Stereo3DScreen *sos);

static void
setRightEyeProjectionMatrix (Stereo3DScreen *sos);

static void
setNoConvergenceProjectionMatrix (Stereo3DScreen *sos);

static void
cleanupProjectionMatrixOperations (Stereo3DScreen *sos);

static Bool
stereo3dDrawWindow (CompWindow           *w,
		    const CompTransform  *transform,
		    const FragmentAttrib *fragment,
		    Region               region,
		    unsigned int         mask)
{
    Bool status;

    STEREO3D_SCREEN(w->screen);
    STEREO3D_WINDOW(w);

    status = TRUE;

    if (sos->enabled)
    {
        mask |= PAINT_WINDOW_TRANSFORMED_MASK;

        initProjectionMatrixChange();

        if (sos->stereoType != 0)
        {
            // ********* left eye *********
            setLeftEyeProjectionMatrix (sos);
            UNWRAP (sos, w->screen, drawWindow);
            status &= (*w->screen->drawWindow) (w, transform, fragment, region, mask);
            WRAP (sos, w->screen, drawWindow, stereo3dDrawWindow);
            if(sow->drawMouse)
            {
                drawCursor(sos);
            }

            // ********* right eye *********
            setRightEyeProjectionMatrix (sos);
            UNWRAP (sos, w->screen, drawWindow);
            status &= (*w->screen->drawWindow) (w, transform, fragment, region, mask);
            WRAP (sos, w->screen, drawWindow, stereo3dDrawWindow);
            if(sow->drawMouse)
            {
                drawCursor(sos);
            }
        }
        else // 2.5D
        {
            setNoConvergenceProjectionMatrix(sos);
            UNWRAP (sos, w->screen, drawWindow);
            status &= (*w->screen->drawWindow) (w, transform, fragment, region, mask);
            WRAP (sos, w->screen, drawWindow, stereo3dDrawWindow);
            if(sow->drawMouse)
            {
                drawCursor(sos);
            }
        }

        // ********* cleanup *********
        cleanupProjectionMatrixOperations(sos);
    }
    else
    {
        UNWRAP (sos, w->screen, drawWindow);
        status &= (*w->screen->drawWindow) (w, transform, fragment, region, mask);
        WRAP (sos, w->screen, drawWindow, stereo3dDrawWindow);
    }

    return status;
}

static void
drawBackgroundWireframe(CompWindow *w, float lightingStrength, float edgesStrength)
{
    STEREO3D_SCREEN (w->screen);
    STEREO3D_WINDOW (w);

    float z1 = 0.0f;
    float z2 = -sow->currAttrs.translation.z;

    float alpha2 = 0.5f * edgesStrength;
    float alpha1 = (alpha2 * 0.8 * (1.0 - sos->lightingStrength) ) * edgesStrength;

    CompTransform sTransform;
    matrixTranslate (&sTransform, 0.0f, 0.0f, -z2);
    transformToScreenSpace (w->screen, &w->screen->outputDev[w->screen->currentOutputDev], -DEFAULT_Z_CAMERA, &sTransform);

    glPushMatrix();
    glLoadMatrixf (sTransform.m);

    float x1 = 0.0f;//-0.5;
    float y1 = 0.0f;//-0.5;

    float x2 = w->screen->width;//0.5f;
    float y2 = w->screen->height;//0.5;

    glEnable( GL_LINE_SMOOTH );
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth (2.0f);

    glBegin( GL_LINES );
    {
        glColor4f( 1.0f, 1.0f, 1.0f, alpha1 );
        glVertex3f( x1, y1, z1  );
        glColor4f( 1.0f, 1.0f, 1.0f, alpha2 );
        glVertex3f( x1, y1, z2 );

        glColor4f( 1.0f, 1.0f, 1.0f, alpha1 );
        glVertex3f( x1, y2, z1 );
        glColor4f( 1.0f, 1.0f, 1.0f, alpha2 );
        glVertex3f( x1, y2, z2 );

        glColor4f( 1.0f, 1.0f, 1.0f, alpha1 );
        glVertex3f( x2, y1, z1 );
        glColor4f( 1.0f, 1.0f, 1.0f, alpha2 );
        glVertex3f( x2, y1, z2 );

        glColor4f( 1.0f, 1.0f, 1.0f, alpha1 );
        glVertex3f( x2, y2, z1 );
        glColor4f( 1.0f, 1.0f, 1.0f, alpha2 );
        glVertex3f( x2, y2, z2 );


        glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    }
    glEnd();
    glDisable( GL_LINE_SMOOTH );


    glPopMatrix();
}

static void
stereo3dDrawWindowTexture (CompWindow           *w,
			   CompTexture          *texture,
			   const FragmentAttrib *attrib,
			   unsigned int         mask)
{
    FragmentAttrib *fa;

    STEREO3D_SCREEN(w->screen);
    STEREO3D_WINDOW(w);

    fa = (FragmentAttrib*)memcpy (malloc (sizeof (FragmentAttrib)), attrib, sizeof (FragmentAttrib));

    if(sos->enabled)
    {
        // switches the eyes
        bool invert = stereo3dGetInvert(w->screen->display);

        switch (sos->renderingState)
        {
            case EyeLeft:
                    sos->currFilter->applyFilter(invert?1:0, fa, texture, w->screen);
                break;

            case EyeRight:
                    sos->currFilter->applyFilter(invert?0:1, fa, texture, w->screen);
                break;

            default:
                break;
        }

        if(sow->floatingType == FTBACKGROUND)
        {
            drawBackgroundWireframe(w, sos->lightingStrength, sos->edgesStrength);
        }
    }

    UNWRAP (sos, w->screen, drawWindowTexture);
    (*w->screen->drawWindowTexture) (w, texture, fa, mask);
    WRAP (sos, w->screen, drawWindowTexture, stereo3dDrawWindowTexture);

    free (fa);
}


static void
initProjectionMatrixChange()
{
//    compLogMessage ("stereoscopic", CompLogLevelError, "initProjectionMatrixChange"  );
    glMatrixMode (GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
}

static float
getWorldZCorrection(float fov)
{
    float tanfov = 0.5f / tan(fov * M_PI / 360.0);

    float result = DEFAULT_Z_CAMERA - (tanfov);

    return result;
}

static void
setLeftEyeProjectionMatrix (Stereo3DScreen *sos)
{
    sos->renderingState = EyeLeft;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();

    glMultMatrixf (sos->projectionL);

    glTranslatef( -(sos->parallax), 0.0f, getWorldZCorrection( stereo3dGetFov(sos->s->display) ) );

    glMatrixMode (GL_MODELVIEW);
}


static void
setRightEyeProjectionMatrix (Stereo3DScreen *sos)
{
    sos->renderingState = EyeRight;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();

    glMultMatrixf (sos->projectionR);

    glTranslatef(sos->parallax, 0.0f, getWorldZCorrection( stereo3dGetFov(sos->s->display) ) );

    glMatrixMode (GL_MODELVIEW);
}


static void
setNoConvergenceProjectionMatrix (Stereo3DScreen *sos)
{
    sos->renderingState = EyeSingle;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();

    glMultMatrixf (sos->projectionM);

    glTranslatef( 0.0f, 0.0f, getWorldZCorrection( stereo3dGetFov(sos->s->display) ) );

    glMatrixMode (GL_MODELVIEW);
}

static void
cleanupProjectionMatrixOperations (Stereo3DScreen *sos)
{
    sos->renderingState = Cleanup;
    glMatrixMode (GL_PROJECTION);
    glPopMatrix();
    glMatrixMode (GL_MODELVIEW);
}

static void
enableMouseDrawing(CompScreen *s);

/********************************************************************
*******************       Handle Inputs       ***********************
*********************************************************************/
static Bool
moveForegroundIn(CompDisplay     *display,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s = findScreenAtDisplay (display, xid);

    if (s) {
        STEREO3D_SCREEN (s);
        return sos->animationMgr.moveForegroundIn();
    }

    return TRUE;
}

static Bool
moveForegroundOut(CompDisplay     *display,
		  CompAction      *action,
		  CompActionState state,
		  CompOption      *option,
		  int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s = findScreenAtDisplay (display, xid);

    if (s) {
        STEREO3D_SCREEN (s);
        return sos->animationMgr.moveForegroundOut();
    }

    return TRUE;
}


static Bool
resetForegroundDepth(CompDisplay     *display,
		     CompAction      *action,
		     CompActionState state,
		     CompOption      *option,
		     int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s = findScreenAtDisplay (display, xid);

    if (s) {
        STEREO3D_SCREEN (s);
        return sos->animationMgr.resetForegroundDepth();
    }

    return TRUE;
}


static Bool
toggleOn (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int             nOption)
{
    CompScreen *s;
    Window xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s = findScreenAtDisplay (d, xid);

    if (s)
    {
	STEREO3D_SCREEN (s);
	sos->enabled = !sos->enabled;

	if(!sos->mouseDrawingEnabled)
	    return true;
    
	if(sos->enabled)
	{
	    if(sos->mouseDrawingEnabled)
		enableMouseDrawing(sos->s);
	}
	else
	{
	    if(sos->mouseDrawingEnabled)
		disableMouseDrawing(sos);
	}
    }
    
    return true;
}

static void
updateMouseInterval (CompScreen *s, int x, int y)
{
    STEREO3D_SCREEN(s);

    sos->animationMgr.setDestMouseX (x);
    sos->animationMgr.setDestMouseY (y);
}


FloatingTypeEnum
getFloatingType (CompWindow *window)
{
    if (window->attrib.override_redirect)
	return FTNONE;

    if (window->attrib.map_state != IsViewable || window->shaded)
        return FTNONE;

    //TODO: move fetching options out of the drawing loop

    if (matchEval (stereo3dGetDesktopMatch (window->screen->display), window))
	return FTBACKGROUND;

    if (matchEval (stereo3dGetDockMatch (window->screen->display), window))
	return FTDOCK;

    if (matchEval (stereo3dGetWindowMatch (window->screen->display), window))
	return FTWINDOW;

    return FTNONE;
}

/********************************************************************
*******************        Constructors       ***********************
*********************************************************************/

static Bool
stereo3dInitScreen (CompPlugin *p,
		    CompScreen *s)
{
    Stereo3DScreen *sos;

    STEREO3D_DISPLAY (s->display);

    sos = (Stereo3DScreen*)calloc (1, sizeof(Stereo3DScreen));
    if (!sos)
        return FALSE;

    sos->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (sos->windowPrivateIndex < 0)
    {
        free (sos);
        return FALSE;
    }

    sos->s = s;
    sos->enabled=(true);
    sos->animPeriod=(1000.0f);
    sos->progress=(0.0f);
    sos->time=(0.0f);
    sos->mouseDrawingEnabled=(true);
    sos->stereoType=(1);
    sos->convergence=(0.0063f);
    sos->parallax=(87.5);

    // register key bindings
    stereo3dSetMoveForegroundInButtonInitiate (s->display, moveForegroundIn);
    stereo3dSetMoveForegroundOutButtonInitiate (s->display, moveForegroundOut);

    stereo3dSetResetForegroundDepthButtonInitiate (s->display, resetForegroundDepth);

    stereo3dSetToggleInitiate (s->display, toggleOn);


    sos->anaglyphFilter.init();
    sos->interlacedFilter.init();


    sos->currFilter = &sos->anaglyphFilter;


    /* draw cursor to texture */
    sos->mouseDrawingEnabled = stereo3dGetDrawmouse(s->display);
    if(sos->mouseDrawingEnabled)
        enableMouseDrawing(s);

    WRAP (sos, s, preparePaintScreen, stereo3dPreparePaintScreen);
    WRAP (sos, s, paintOutput, stereo3dPaintOutput);
    WRAP (sos, s, paintTransformedOutput, stereo3dPaintTransformedOutput);
    WRAP (sos, s, donePaintScreen, stereo3dDonePaintScreen);
    WRAP (sos, s, drawWindow, stereo3dDrawWindow);
    WRAP (sos, s, drawWindowTexture, stereo3dDrawWindowTexture);

    s->base.privates[sod->screenPrivateIndex].ptr = sos;

    return TRUE;
}

static void
enableMouseDrawing(CompScreen *s)
{
    int x, y;

    STEREO3D_DISPLAY(s->display);
    STEREO3D_SCREEN(s);

    updateCursor(s);

    //hides original cursor
    XFixesHideCursor (s->display->display, s->root);
    
    sos->pollHandle = sod->mpFunc->addPositionPolling (s, updateMouseInterval);
    //lastChange = time(NULL);
    sod->mpFunc->getCurrentPosition (s, &x, &y);
    sos->animationMgr.setDestMouseX((float) x);
    sos->animationMgr.setDestMouseY((float) y);
}

static void
disableMouseDrawing(Stereo3DScreen *sos)
{
    STEREO3D_DISPLAY(sos->s->display);

    sod->mpFunc->removePositionPolling (sos->s, sos->pollHandle);
    XFixesShowCursor (sos->s->display->display, sos->s->root);
    freeCursor (sos, &sos->cursorTex);
}

static void
stereo3dFiniScreen (CompPlugin *p,
		    CompScreen *s)
{
    STEREO3D_SCREEN (s);

    freeWindowPrivateIndex (s, sos->windowPrivateIndex);

    sos -> anaglyphFilter.deinit(s);
    sos -> interlacedFilter.deinit(s);

    if(sos->mouseDrawingEnabled)
        disableMouseDrawing(sos);

    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDisable (GL_STENCIL_TEST);

    UNWRAP (sos, s, preparePaintScreen);
    UNWRAP (sos, s, paintOutput);
    UNWRAP (sos, s, paintTransformedOutput);
    UNWRAP (sos, s, donePaintScreen);
    UNWRAP (sos, s, drawWindow);
    UNWRAP (sos, s, drawWindowTexture);

    free(sos);
}

static Bool
stereo3dInitWindow (CompPlugin *p, CompWindow *w)
{
    Stereo3DWindow *sow;
    STEREO3D_SCREEN(w->screen);

    sow = (Stereo3DWindow*)calloc (1, sizeof (Stereo3DWindow));
    if (!sow)
        return FALSE;

    sow->window = w;
    sow->drawMouse = false;
    sow->floatingType = FTNONE;

    sow->floatingType = getFloatingType(w);

    sow->currAttrs.rotation.x=0.0f;
    sow->currAttrs.rotation.y=0.0f;
    sow->currAttrs.rotation.z=0.0f;

    sow->currAttrs.translation.x=0.0f;
    sow->currAttrs.translation.y=0.0f;
    sow->currAttrs.translation.z=0.0f;

    sow->currAttrs.scale=1.0f;

    sow->dstAttrs.rotation.x=0.0f;
    sow->dstAttrs.rotation.y=0.0f;
    sow->dstAttrs.rotation.z=0.0f;

    sow->dstAttrs.translation.x=0.0f;
    sow->dstAttrs.translation.y=0.0f;
    sow->dstAttrs.translation.z=0.0f;

    sow->dstAttrs.scale=1.0f;

    w->base.privates[sos->windowPrivateIndex].ptr = sow;

    return TRUE;
}

static void
stereo3dFiniWindow (CompPlugin *p, CompWindow *w)
{
    STEREO3D_WINDOW(w);
    free(sow);
}

static Bool
stereo3dInitDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    int index;
    Stereo3DDisplay *sod;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
        return FALSE;

    if (!checkPluginABI ("mousepoll", MOUSEPOLL_ABIVERSION))
        return FALSE;

    if (!getPluginDisplayIndex (d, "mousepoll", &index))
        return FALSE;

    sod = (Stereo3DDisplay*) malloc (sizeof (Stereo3DDisplay));
    if (!sod)
        return FALSE;

    sod->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sod->screenPrivateIndex < 0)
    {
	free (sod);
	return FALSE;
    }

    sod->mpFunc = (MousePollFunc*) d->base.privates[index].ptr;

    d->base.privates[displayPrivateIndex].ptr = sod;

    return TRUE;
}

static void
stereo3dFiniDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    STEREO3D_DISPLAY (d);

    freeScreenPrivateIndex (d, sod->screenPrivateIndex);
    free (sod);
}

static Bool
stereo3dInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();

    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static CompBool
stereo3dInitObject (CompPlugin *p,
		    CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0,
	(InitPluginObjectProc) stereo3dInitDisplay,
	(InitPluginObjectProc) stereo3dInitScreen,
	(InitPluginObjectProc) stereo3dInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
stereo3dFiniObject (CompPlugin *p,
		    CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0,
	(FiniPluginObjectProc) stereo3dFiniDisplay,
	(FiniPluginObjectProc) stereo3dFiniScreen,
	(FiniPluginObjectProc) stereo3dFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static void
stereo3dFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable stereo3dVTable = {
    "stereo3d",
    0,
    stereo3dInit,
    stereo3dFini,
    stereo3dInitObject,
    stereo3dFiniObject,
    0,
    0,
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &stereo3dVTable;
}
