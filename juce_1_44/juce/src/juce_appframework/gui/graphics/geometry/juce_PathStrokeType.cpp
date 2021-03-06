/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

#include "../../../../juce_core/basics/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE


#include "juce_PathStrokeType.h"
#include "juce_PathIterator.h"
#include "../../../../juce_core/containers/juce_VoidArray.h"


//==============================================================================
PathStrokeType::PathStrokeType (const float strokeThickness,
                                const JointStyle jointStyle_,
                                const EndCapStyle endStyle_) throw()
    : thickness (strokeThickness),
      jointStyle (jointStyle_),
      endStyle (endStyle_)
{
}

PathStrokeType::PathStrokeType (const PathStrokeType& other) throw()
    : thickness (other.thickness),
      jointStyle (other.jointStyle),
      endStyle (other.endStyle)
{
}

const PathStrokeType& PathStrokeType::operator= (const PathStrokeType& other) throw()
{
    thickness = other.thickness;
    jointStyle = other.jointStyle;
    endStyle = other.endStyle;
    return *this;
}

PathStrokeType::~PathStrokeType() throw()
{
}

bool PathStrokeType::operator== (const PathStrokeType& other) const throw()
{
    return thickness == other.thickness
        && jointStyle == other.jointStyle
        && endStyle == other.endStyle;
}

bool PathStrokeType::operator!= (const PathStrokeType& other) const throw()
{
    return ! operator== (other);
}

//==============================================================================
static bool lineIntersection (const float x1, const float y1,
                              const float x2, const float y2,
                              const float x3, const float y3,
                              const float x4, const float y4,
                              float& intersectionX,
                              float& intersectionY,
                              float& distanceBeyondLine1EndSquared) throw()
{
    if (x2 != x3 || y2 != y3)
    {
        const float dx1 = x2 - x1;
        const float dy1 = y2 - y1;
        const float dx2 = x4 - x3;
        const float dy2 = y4 - y3;
        const float divisor = dx1 * dy2 - dx2 * dy1;

        if (divisor == 0)
        {
            if (! ((dx1 == 0 && dy1 == 0) || (dx2 == 0 && dy2 == 0)))
            {
                if (dy1 == 0 && dy2 != 0)
                {
                    const float along = (y1 - y3) / dy2;
                    intersectionX = x3 + along * dx2;
                    intersectionY = y1;

                    distanceBeyondLine1EndSquared = intersectionX - x2;
                    distanceBeyondLine1EndSquared *= distanceBeyondLine1EndSquared;
                    if ((x2 > x1) == (intersectionX < x2))
                        distanceBeyondLine1EndSquared = -distanceBeyondLine1EndSquared;

                    return along >= 0 && along <= 1.0f;
                }
                else if (dy2 == 0 && dy1 != 0)
                {
                    const float along = (y3 - y1) / dy1;
                    intersectionX = x1 + along * dx1;
                    intersectionY = y3;

                    distanceBeyondLine1EndSquared = (along - 1.0f) * dx1;
                    distanceBeyondLine1EndSquared *= distanceBeyondLine1EndSquared;
                    if (along < 1.0f)
                        distanceBeyondLine1EndSquared = -distanceBeyondLine1EndSquared;

                    return along >= 0 && along <= 1.0f;
                }
                else if (dx1 == 0 && dx2 != 0)
                {
                    const float along = (x1 - x3) / dx2;
                    intersectionX = x1;
                    intersectionY = y3 + along * dy2;

                    distanceBeyondLine1EndSquared = intersectionY - y2;
                    distanceBeyondLine1EndSquared *= distanceBeyondLine1EndSquared;

                    if ((y2 > y1) == (intersectionY < y2))
                        distanceBeyondLine1EndSquared = -distanceBeyondLine1EndSquared;

                    return along >= 0 && along <= 1.0f;
                }
                else if (dx2 == 0 && dx1 != 0)
                {
                    const float along = (x3 - x1) / dx1;
                    intersectionX = x3;
                    intersectionY = y1 + along * dy1;

                    distanceBeyondLine1EndSquared = (along - 1.0f) * dy1;
                    distanceBeyondLine1EndSquared *= distanceBeyondLine1EndSquared;
                    if (along < 1.0f)
                        distanceBeyondLine1EndSquared = -distanceBeyondLine1EndSquared;

                    return along >= 0 && along <= 1.0f;
                }
            }

            intersectionX = 0.5f * (x2 + x3);
            intersectionY = 0.5f * (y2 + y3);

            distanceBeyondLine1EndSquared = 0.0f;
            return false;
        }
        else
        {
            const float along1 = ((y1 - y3) * dx2 - (x1 - x3) * dy2) / divisor;

            intersectionX = x1 + along1 * dx1;
            intersectionY = y1 + along1 * dy1;

            if (along1 >= 0 && along1 <= 1.0f)
            {
                const float along2 = ((y1 - y3) * dx1 - (x1 - x3) * dy1);

                if (along2 >= 0 && along2 <= divisor)
                {
                    distanceBeyondLine1EndSquared = 0.0f;
                    return true;
                }
            }

            distanceBeyondLine1EndSquared = along1 - 1.0f;
            distanceBeyondLine1EndSquared *= distanceBeyondLine1EndSquared;
            distanceBeyondLine1EndSquared *= (dx1 * dx1 + dy1 * dy1);

            if (along1 < 1.0f)
                distanceBeyondLine1EndSquared = -distanceBeyondLine1EndSquared;

            return false;
        }
    }

    intersectionX = x2;
    intersectionY = y2;

    distanceBeyondLine1EndSquared = 0.0f;
    return true;
}

// part of stroke drawing stuff
static void addEdgeAndJoint (Path& destPath,
                             const PathStrokeType::JointStyle style,
                             const float maxMiterExtensionSquared, const float width,
                             const float x1, const float y1,
                             const float x2, const float y2,
                             const float x3, const float y3,
                             const float x4, const float y4,
                             const float midX, const float midY) throw()
{
    if (style == PathStrokeType::beveled
        || (x3 == x4 && y3 == y4)
        || (x1 == x2 && y1 == y2))
    {
        destPath.lineTo (x2, y2);
        destPath.lineTo (x3, y3);
    }
    else
    {
        float jx, jy, distanceBeyondLine1EndSquared;

        // if they intersect, use this point..
        if (lineIntersection (x1, y1, x2, y2,
                              x3, y3, x4, y4,
                              jx, jy, distanceBeyondLine1EndSquared))
        {
            destPath.lineTo (jx, jy);
        }
        else
        {
            if (style == PathStrokeType::mitered)
            {
                if (distanceBeyondLine1EndSquared < maxMiterExtensionSquared
                    && distanceBeyondLine1EndSquared > 0.0f)
                {
                    destPath.lineTo (jx, jy);
                }
                else
                {
                    // the end sticks out too far, so just use a blunt joint
                    destPath.lineTo (x2, y2);
                    destPath.lineTo (x3, y3);
                }
            }
            else
            {
                // curved joints
                float angle  = atan2f (x2 - midX, y2 - midY);
                float angle2 = atan2f (x3 - midX, y3 - midY);

                while (angle < angle2 - 0.01f)
                    angle2 -= float_Pi * 2.0f;

                destPath.lineTo (x2, y2);

                while (angle > angle2)
                {
                    destPath.lineTo (midX + width * sinf (angle),
                                     midY + width * cosf (angle));

                    angle -= 0.1f;
                }

                destPath.lineTo (x3, y3);
            }
        }
    }
}

static inline void addLineEnd (Path& destPath,
                               const PathStrokeType::EndCapStyle style,
                               const float x1, const float y1,
                               const float x2, const float y2,
                               const float width) throw()
{
    if (style == PathStrokeType::butt)
    {
        destPath.lineTo (x2, y2);
    }
    else
    {
        float offx1, offy1, offx2, offy2;

        float dx = x2 - x1;
        float dy = y2 - y1;
        const float len = juce_hypotf (dx, dy);

        if (len == 0)
        {
            offx1 = offx2 = x1;
            offy1 = offy2 = y1;
        }
        else
        {
            const float offset = width / len;
            dx *= offset;
            dy *= offset;

            offx1 = x1 + dy;
            offy1 = y1 - dx;
            offx2 = x2 + dy;
            offy2 = y2 - dx;
        }

        if (style == PathStrokeType::square)
        {
            // sqaure ends
            destPath.lineTo (offx1, offy1);
            destPath.lineTo (offx2, offy2);
            destPath.lineTo (x2, y2);
        }
        else
        {
            // rounded ends
            const float midx = (offx1 + offx2) * 0.5f;
            const float midy = (offy1 + offy2) * 0.5f;

            destPath.cubicTo (x1 + (offx1 - x1) * 0.55f, y1 + (offy1 - y1) * 0.55f,
                              offx1 + (midx - offx1) * 0.45f, offy1 + (midy - offy1) * 0.45f,
                              midx, midy);

            destPath.cubicTo (midx + (offx2 - midx) * 0.55f, midy + (offy2 - midy) * 0.55f,
                              offx2 + (x2 - offx2) * 0.45f, offy2 + (y2 - offy2) * 0.45f,
                              x2, y2);
        }
    }
}

struct LineSection
{
    LineSection() throw()       {}
    LineSection (int) throw()   {}

    float x1, y1, x2, y2;      // original line
    float lx1, ly1, lx2, ly2;  // the left-hand stroke
    float rx1, ry1, rx2, ry2;  // the right-hand stroke
};

static void addSubPath (Path& destPath, const Array <LineSection>& subPath,
                        const bool isClosed,
                        const float width, const float maxMiterExtensionSquared,
                        const PathStrokeType::JointStyle jointStyle, const PathStrokeType::EndCapStyle endStyle) throw()
{
    jassert (subPath.size() > 0);

    const LineSection& firstLine = subPath.getReference (0);

    float lastX1 = firstLine.lx1;
    float lastY1 = firstLine.ly1;
    float lastX2 = firstLine.lx2;
    float lastY2 = firstLine.ly2;

    if (isClosed)
    {
        destPath.startNewSubPath (lastX1, lastY1);
    }
    else
    {
        destPath.startNewSubPath (firstLine.rx2, firstLine.ry2);

        addLineEnd (destPath, endStyle,
                    firstLine.rx2, firstLine.ry2,
                    lastX1, lastY1,
                    width);
    }

    int i;
    for (i = 1; i < subPath.size(); ++i)
    {
        const LineSection& l = subPath.getReference (i);

        addEdgeAndJoint (destPath, jointStyle,
                         maxMiterExtensionSquared, width,
                         lastX1, lastY1, lastX2, lastY2,
                         l.lx1, l.ly1, l.lx2, l.ly2,
                         l.x1, l.y1);

        lastX1 = l.lx1;
        lastY1 = l.ly1;
        lastX2 = l.lx2;
        lastY2 = l.ly2;
    }

    const LineSection& lastLine = subPath.getReference (subPath.size() - 1);

    if (isClosed)
    {
        const LineSection& l = subPath.getReference (0);

        addEdgeAndJoint (destPath, jointStyle,
                         maxMiterExtensionSquared, width,
                         lastX1, lastY1, lastX2, lastY2,
                         l.lx1, l.ly1, l.lx2, l.ly2,
                         l.x1, l.y1);

        destPath.closeSubPath();
        destPath.startNewSubPath (lastLine.rx1, lastLine.ry1);
    }
    else
    {
        destPath.lineTo (lastX2, lastY2);

        addLineEnd (destPath, endStyle,
                    lastX2, lastY2,
                    lastLine.rx1, lastLine.ry1,
                    width);
    }

    lastX1 = lastLine.rx1;
    lastY1 = lastLine.ry1;
    lastX2 = lastLine.rx2;
    lastY2 = lastLine.ry2;

    for (i = subPath.size() - 1; --i >= 0;)
    {
        const LineSection& l = subPath.getReference (i);

        addEdgeAndJoint (destPath, jointStyle,
                         maxMiterExtensionSquared, width,
                         lastX1, lastY1, lastX2, lastY2,
                         l.rx1, l.ry1, l.rx2, l.ry2,
                         l.x2, l.y2);

        lastX1 = l.rx1;
        lastY1 = l.ry1;
        lastX2 = l.rx2;
        lastY2 = l.ry2;
    }

    if (isClosed)
    {
        addEdgeAndJoint (destPath, jointStyle,
                         maxMiterExtensionSquared, width,
                         lastX1, lastY1, lastX2, lastY2,
                         lastLine.rx1, lastLine.ry1, lastLine.rx2, lastLine.ry2,
                         lastLine.x2, lastLine.y2);
    }
    else
    {
        // do the last line
        destPath.lineTo (lastX2, lastY2);
    }

    destPath.closeSubPath();
}

void PathStrokeType::createStrokedPath (Path& destPath,
                                        const Path& source,
                                        const AffineTransform& transform,
                                        const float extraAccuracy) const throw()
{
    if (thickness <= 0)
    {
        destPath.clear();
        return;
    }

    const Path* sourcePath = &source;
    Path temp;

    if (sourcePath == &destPath)
    {
        destPath.swapWithPath (temp);
        sourcePath = &temp;
    }
    else
    {
        destPath.clear();
    }

    destPath.setUsingNonZeroWinding (true);

    const float maxMiterExtensionSquared = 9.0f * thickness * thickness;
    const float width = 0.5f * thickness;

    // Iterate the path, creating a list of the
    // left/right-hand lines along either side of it...
    PathFlatteningIterator it (*sourcePath, transform, 9.0f / extraAccuracy);

    Array <LineSection> subPath;
    LineSection l;
    l.x1 = 0;
    l.y1 = 0;

    const float minSegmentLength = 2.0f / (extraAccuracy * extraAccuracy);

    while (it.next())
    {
        if (it.subPathIndex == 0)
        {
            if (subPath.size() > 0)
            {
                addSubPath (destPath, subPath, false, width, maxMiterExtensionSquared, jointStyle, endStyle);
                subPath.clearQuick();
            }

            l.x1 = it.x1;
            l.y1 = it.y1;
        }

        l.x2 = it.x2;
        l.y2 = it.y2;

        float dx = l.x2 - l.x1;
        float dy = l.y2 - l.y1;

        const float hypotSquared = dx*dx + dy*dy;

        if (it.closesSubPath || hypotSquared > minSegmentLength)
        {
            const float len = sqrtf (hypotSquared);

            if (len == 0)
            {
                l.rx1 = l.rx2 = l.lx1 = l.lx2 = l.x1;
                l.ry1 = l.ry2 = l.ly1 = l.ly2 = l.y1;
            }
            else
            {
                const float offset = width / len;
                dx *= offset;
                dy *= offset;

                l.rx2 = l.x1 - dy;
                l.ry2 = l.y1 + dx;
                l.lx1 = l.x1 + dy;
                l.ly1 = l.y1 - dx;

                l.lx2 = l.x2 + dy;
                l.ly2 = l.y2 - dx;
                l.rx1 = l.x2 - dy;
                l.ry1 = l.y2 + dx;
            }

            subPath.add (l);

            if (it.closesSubPath)
            {
                addSubPath (destPath, subPath, true, width, maxMiterExtensionSquared, jointStyle, endStyle);
                subPath.clearQuick();
            }
            else
            {
                l.x1 = it.x2;
                l.y1 = it.y2;
            }
        }
    }

    if (subPath.size() > 0)
        addSubPath (destPath, subPath, false, width, maxMiterExtensionSquared, jointStyle, endStyle);
}

void PathStrokeType::createDashedStroke (Path& destPath,
                                         const Path& sourcePath,
                                         const float* dashLengths,
                                         int numDashLengths,
                                         const AffineTransform& transform,
                                         const float extraAccuracy) const throw()
{
    if (thickness <= 0)
        return;

    // this should really be an even number..
    jassert ((numDashLengths & 1) == 0);

    Path newDestPath;
    PathFlatteningIterator it (sourcePath, transform, 9.0f / extraAccuracy);

    bool first = true;
    int dashNum = 0;
    float pos = 0.0f, lineLen = 0.0f, lineEndPos = 0.0f;
    float dx = 0.0f, dy = 0.0f;

    for (;;)
    {
        const bool isSolid = ((dashNum & 1) == 0);

        const float dashLen = dashLengths [dashNum++ % numDashLengths];
        jassert (dashLen > 0); // must be a positive increment!
        if (dashLen <= 0)
            break;

        pos += dashLen;

        while (pos > lineEndPos)
        {
            if (! it.next())
            {
                if (isSolid && ! first)
                    newDestPath.lineTo (it.x2, it.y2);

                createStrokedPath (destPath, newDestPath, AffineTransform::identity, extraAccuracy);
                return;
            }

            if (isSolid && ! first)
            {
                newDestPath.lineTo (it.x1, it.y1);
            }
            else
            {
                newDestPath.startNewSubPath (it.x1, it.y1);
                first = false;
            }

            dx = it.x2 - it.x1;
            dy = it.y2 - it.y1;
            lineLen = juce_hypotf (dx, dy);
            lineEndPos += lineLen;
        }

        const float alpha = (pos - (lineEndPos - lineLen)) / lineLen;

        if (isSolid)
            newDestPath.lineTo (it.x1 + dx * alpha,
                                it.y1 + dy * alpha);
        else
            newDestPath.startNewSubPath (it.x1 + dx * alpha,
                                         it.y1 + dy * alpha);
    }
}


END_JUCE_NAMESPACE
