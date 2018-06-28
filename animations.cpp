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

void _AnimationManager::updateWindowsPosition(CompWindow* windows, float depth, float lightingStrength)
{
    int floatingWindowsCount = 0;

    backgroundDepth = depth;

    updateMousePosition();
    Stereo3DWindow *lastDrawnWindow = NULL;
    Stereo3DWindow *bkgWindow = NULL;


    for (CompWindow *w = windows; w; w = w->next)
    {
        STEREO3D_WINDOW (w);
        sow->floatingType = getFloatingType(w);

        if(sow->floatingType == FTWINDOW)
            floatingWindowsCount ++;
    }
    if(floatingWindowsCount == 0 ) floatingWindowsCount++;

    //compLogMessage ("stereo3d", CompLogLevelWarn, "WndCount: %d", windowsCount);

    float step = (depth + foregroundCurrZ) / (float)floatingWindowsCount;

    int i = 0;
    for (CompWindow *w = windows; w; w = w->next)
    {
        STEREO3D_WINDOW (w);
        
        sow -> drawMouse = false;
        sow -> opacity = 1.0f;
        sow -> brightness = 1.0f;
        sow -> saturation = 1.0f;

        switch(sow -> floatingType)
        {
        case FTBACKGROUND:
            sow ->dstAttrs.translation.z = -depth ;
            sow ->dstAttrs.rotation.y = 0.0f;

            
            sow -> brightness = 1.0f - 0.8 * lightingStrength;
            sow -> saturation = 1.0f - 0.5 * lightingStrength;
            

            bkgWindow = sow;
            break;

        case FTDOCK:
            sow ->dstAttrs.translation.z = 0.0f;
            sow ->dstAttrs.rotation.y = 0.0f;
//            lastDrawnWindow = sow;
            break;

        case FTWINDOW:
        {
            float wndDepth = depth - (float)(i+1) * step;
            if (wndDepth > depth ) wndDepth = depth;
            lastDrawnWindow = sow;
            
            
            sow -> brightness = 1.0f - (wndDepth/depth)* 0.8 * lightingStrength;
            sow -> saturation = 1.0f - (wndDepth/depth)* 0.5 * lightingStrength;

            if(sow -> brightness>1.0f) sow -> brightness = 1.0f;
            if(sow -> saturation>1.0f) sow -> saturation = 1.0f;
            

            sow ->dstAttrs.translation.z = -wndDepth;
            sow ->dstAttrs.rotation.y = 0.0f;

            i ++;
        }
        break;
        default:
            break;
        }

        /** update animation current positions **/
        updateWindow(sow);
    }
    

    if(lastDrawnWindow != NULL)
        lastDrawnWindow->drawMouse = true;
    else if(bkgWindow != NULL)
        bkgWindow->drawMouse = true;
    else
        compLogMessage ("stereo3d", CompLogLevelWarn, "no window found to hook up mouse drawing");        

}

void _AnimationManager::updateWindow(Stereo3DWindow * sow)
{
    sow ->currAttrs.rotation.x += ( sow->dstAttrs.rotation.x - sow->currAttrs.rotation.x ) / 2.0f;
    sow ->currAttrs.rotation.y += ( sow->dstAttrs.rotation.y - sow->currAttrs.rotation.y ) / 2.0f;
    sow ->currAttrs.rotation.z += ( sow->dstAttrs.rotation.z - sow->currAttrs.rotation.z ) / 2.0f;

    sow ->currAttrs.translation.x += ( sow->dstAttrs.translation.x - sow->currAttrs.translation.x ) / 2.0f;
    sow ->currAttrs.translation.y += ( sow->dstAttrs.translation.y - sow->currAttrs.translation.y ) / 2.0f;
    sow ->currAttrs.translation.z += ( sow->dstAttrs.translation.z - sow->currAttrs.translation.z ) / 2.0f;

    sow ->currAttrs.scale += ( sow->dstAttrs.scale - sow->currAttrs.scale ) / 2.0f;
}

void _AnimationManager::updateMousePosition() 
{
    mouseCurr.x += (mouseDst.x - mouseCurr.x)/2.0f;
    mouseCurr.y += (mouseDst.y - mouseCurr.y)/2.0f;
    foregroundCurrZ = foregroundCurrZ + (foregroundDstZ - foregroundCurrZ)/1.5f;
}

bool
_AnimationManager::moveForegroundIn()
{

    if(foregroundDstZ - 0.02f  > -backgroundDepth )
        foregroundDstZ -= 0.02f;
                
    return true;
}

bool
_AnimationManager::moveForegroundOut()
{    
    float foregroundLimit = 0.18f;

    if(foregroundDstZ + 0.02f  < foregroundLimit )
        foregroundDstZ += 0.02f;
    else
        foregroundDstZ = foregroundLimit;
    
    return true;
}

bool
_AnimationManager::resetForegroundDepth()
{
    foregroundDstZ = 0.0f;
    return true;
}


float _AnimationManager::getCurrentForegroundZ()
{
    return foregroundCurrZ;
}

float _AnimationManager::getCurrentMouseX()
{
    return mouseCurr.x;
}


float _AnimationManager::getCurrentMouseY()
{
    return mouseCurr.y;
}


void _AnimationManager::setDestMouseX(float value)
{
    mouseDst.x = value;
}


void _AnimationManager::setDestMouseY(float value)
{
    mouseDst.y = value;
}
