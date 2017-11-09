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

void InterlacedFilter::init()
{
    column = false;
}

void InterlacedFilter::applyFilter(int eyenum, FragmentAttrib *fa, CompTexture *texture, CompScreen *s)
{
    if(eyenum==0)
        glStencilFunc(GL_NOTEQUAL, 0, 1);
    else
        glStencilFunc(GL_EQUAL, 0, 1);
}

void InterlacedFilter::prepareFilter(int width, int height)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity ();
    glOrtho (0, width, height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glEnable (GL_STENCIL_TEST);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    glStencilMask(GL_TRUE);

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilOp(GL_INVERT, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_NEVER, 1, 1);

    glDisable( GL_LINE_SMOOTH );
    glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
    glLineWidth (1.0f);


    glBegin(GL_LINES);
    {
        if(!column)
        {
            int y;
            for (y=0 ; y <= height; y += 2) {
                glVertex2f(0.0f, float(y) ) ;
                glVertex2f((float)width, float(y) );
            }
        }
        else
        {
            int x;
            for (x=0 ; x <= width; x += 2) {
                glVertex2f(float(x), 0.0f) ;
                glVertex2f(float(x), (float)width);
            }
        }
    }
    glEnd();

    glStencilMask(GL_FALSE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    //glDisable (GL_STENCIL_TEST);

    glStencilFunc(GL_NOTEQUAL, 0, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    
    glPopMatrix();
//
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void InterlacedFilter::cleanup()
{
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
}


void
AnaglyphFilter::init()
{
    for(int i=0; i<3; i++)
    {
        this->fragmentFunctions[i] = 0;
    }
}


void
AnaglyphFilter::deinit(CompScreen *s)
{
    for(int i=0; i<3; i++)
    {
        if(this->fragmentFunctions[i] != 0)
        {
            destroyFragmentFunction(s, this->fragmentFunctions[i]);
            this->fragmentFunctions[i] = 0;
        }
    }
}

void
AnaglyphFilter::applyFilter(int eyenum, FragmentAttrib *fa, CompTexture *texture, CompScreen *s)
{
    if(eyenum==0)
    {
    //    compLogMessage ("stereoscopic", CompLogLevelError, "setLeftEyeFilter");
        int fragmentId;

        glColorMask (GL_FALSE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(GLenum err = glGetError () != GL_NO_ERROR)
            compLogMessage ("stereoscopic", CompLogLevelWarn, "glColorMask problem! %d", err );

        /* Anaglif Left */
        fragmentId = getAnaglifFragmentFunction(texture, s);

        addFragmentFunction(fa, fragmentId);
    }
    else
    {

//    compLogMessage ("stereoscopic", CompLogLevelError, "setRightEyeFilter");
        int fragmentId;

        glColorMask (GL_TRUE,GL_FALSE,GL_FALSE,GL_TRUE);
        if(GLenum err = glGetError () != GL_NO_ERROR)
            compLogMessage ("stereoscopic", CompLogLevelWarn, "glColorMask problem! %d", err  );

        /* Anaglif Left */
        fragmentId = getAnaglifFragmentFunction(texture, s);

        addFragmentFunction(fa, fragmentId);
    }

}

int
AnaglyphFilter::getAnaglifFragmentFunction (CompTexture *texture, CompScreen *s)
{
    int      target;
    Bool     status = TRUE;

    if (texture->target == GL_TEXTURE_2D)
        target = COMP_FETCH_TARGET_2D;
    else
        target = COMP_FETCH_TARGET_RECT;

    if (!this->fragmentFunctions[target])
    {
        static const char *anaglifProgramData =
		"MOV temp, output;"
		"DP3 temp.r, output, {0.1, 0.63, 0.27, 0.0};" //optimized anaglyph
		"DP3 temp.g, output, {0.1, 0.9 , 0.0 , 0.0};"
		"DP3 temp.b, output, {0.1, 0.0 , 0.9 , 0.0};"
		"MOV output, temp;";
        CompFunctionData *data;
        data = createFunctionData ();

	status &= addTempHeaderOpToFunctionData (data, "temp");
	status &= addFetchOpToFunctionData (data, "output", NULL, target);
	status &= addColorOpToFunctionData (data, "output", "output");

	status &= addDataOpToFunctionData (data, anaglifProgramData);

        if (!status)
        {
            compLogMessage ("stereoscopic", CompLogLevelWarn, "Error creating fragment program");
            return 0;
        }

        this->fragmentFunctions[target] = createFragmentFunction (s, "stereoscopic_anaglif", data);
        printf("####### Creating fragment function... ##### \n");
        destroyFunctionData (data);
    }

    return this->fragmentFunctions[target];
}

void AnaglyphFilter::prepareFilter(int width, int height)
{
    
}

void AnaglyphFilter::cleanup()
{
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
}

