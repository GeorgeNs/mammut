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

#include "../../../juce_Config.h"
#if JUCE_BUILD_GUI_CLASSES

#include "linuxincludes.h"

/*  Got a build error here? You'll need to install the freetype library...

    The name of the package to install is "libfreetype6-dev".
*/
#include <ft2build.h>
#include FT_FREETYPE_H

#include "../../../src/juce_core/basics/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE

#include "../../../src/juce_appframework/gui/graphics/fonts/juce_Font.h"
#include "../../../src/juce_core/basics/juce_Singleton.h"
#include "../../../src/juce_core/io/streams/juce_MemoryInputStream.h"
#include "../../../src/juce_core/io/files/juce_DirectoryIterator.h"
#include "../../../src/juce_core/text/juce_XmlDocument.h"
#include "../../../src/juce_appframework/application/juce_DeletedAtShutdown.h"


//==============================================================================
class FreeTypeFontFace
{
public:
    //==============================================================================
    enum FontStyle
    {
        Plain = 0,
        Bold = 1,
        Italic = 2
    };

    struct FontNameIndex
    {
        String fileName;
        int faceIndex;
    };

    //==============================================================================
    FreeTypeFontFace (const String& familyName)
      : hasSerif (false),
        monospaced (false)
    {
        family = familyName;
    }

    void setFileName (const String& name,
                      const int faceIndex,
                      FontStyle style)
    {
        if (names[(int) style].fileName.isEmpty())
        {
            names[(int) style].fileName = name;
            names[(int) style].faceIndex = faceIndex;
        }
    }

    const String& getFamilyName() const throw()
    {
        return family;
    }

    const String& getFileName (int style, int* faceIndex) const throw()
    {
        *faceIndex = names [style].faceIndex;
        return names[style].fileName;
    }

    void setMonospaced (bool mono)               { monospaced = mono; }
    bool getMonospaced () const throw()          { return monospaced; }

    void setSerif (const bool serif)             { hasSerif = serif; }
    bool getSerif () const throw()               { return hasSerif; }

private:
    //==============================================================================
    String family;
    FontNameIndex names[4];
    bool hasSerif, monospaced;
};

//==============================================================================
class FreeTypeInterface  : public DeletedAtShutdown
{
public:
    //==============================================================================
    FreeTypeInterface() throw()
        : lastFace (0),
          lastBold (false),
          lastItalic (false)
    {
        if (FT_Init_FreeType (&ftLib) != 0)
        {
            ftLib = 0;
            DBG (T("Failed to initialize FreeType"));
        }

        StringArray fontDirs;
        fontDirs.addTokens (String (getenv ("JUCE_FONT_PATH")), T(";,"), 0);
        fontDirs.removeEmptyStrings (true);

        if (fontDirs.size() == 0)
        {
            XmlDocument fontsConfig (File ("/etc/fonts/fonts.conf"));
            XmlElement* const fontsInfo = fontsConfig.getDocumentElement();

            if (fontsInfo != 0)
            {
                forEachXmlChildElementWithTagName (*fontsInfo, e, T("dir"))
                {
                    fontDirs.add (e->getAllSubText().trim());
                }

                delete fontsInfo;
            }
        }

        if (fontDirs.size() == 0)
            fontDirs.add ("/usr/X11R6/lib/X11/fonts");

        for (int i = 0; i < fontDirs.size(); ++i)
            enumerateFaces (fontDirs[i]);
    }

    ~FreeTypeInterface() throw()
    {
        if (lastFace != 0)
            FT_Done_Face (lastFace);

        if (ftLib != 0)
            FT_Done_FreeType (ftLib);

        clearSingletonInstance();
    }

    //==============================================================================
    FreeTypeFontFace* findOrCreate (const String& familyName,
                                    const bool create = false) throw()
    {
        for (int i = 0; i < faces.size(); i++)
            if (faces[i]->getFamilyName() == familyName)
                return faces[i];

        if (! create)
            return NULL;

        FreeTypeFontFace* newFace = new FreeTypeFontFace (familyName);
        faces.add (newFace);

        return newFace;
    }

    // Enumerate all font faces available in a given directory
    void enumerateFaces (const String& path) throw()
    {
        File dirPath (path);
        if (path.isEmpty() || ! dirPath.isDirectory())
            return;

        DirectoryIterator di (dirPath, true);

        while (di.next())
        {
            File possible (di.getFile());

            if (possible.hasFileExtension (T("ttf"))
                 || possible.hasFileExtension (T("pfb"))
                 || possible.hasFileExtension (T("pcf")))
            {
                FT_Face face;
                int faceIndex = 0;
                int numFaces = 0;

                do
                {
                    if (FT_New_Face (ftLib,
                                     possible.getFullPathName(),
                                     faceIndex,
                                     &face) == 0)
                    {
                        if (faceIndex == 0)
                            numFaces = face->num_faces;

                        if ((face->face_flags & FT_FACE_FLAG_SCALABLE) != 0)
                        {
                            FreeTypeFontFace* const newFace = findOrCreate (face->family_name, true);
                            int style = (int) FreeTypeFontFace::Plain;

                            if ((face->style_flags & FT_STYLE_FLAG_BOLD) != 0)
                                style |= (int) FreeTypeFontFace::Bold;

                            if ((face->style_flags & FT_STYLE_FLAG_ITALIC) != 0)
                                style |= (int) FreeTypeFontFace::Italic;

                            newFace->setFileName (possible.getFullPathName(), faceIndex, (FreeTypeFontFace::FontStyle) style);

                            if ((face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) != 0)
                                newFace->setMonospaced (true);
                            else
                                newFace->setMonospaced (false);

                            // Surely there must be a better way to do this?
                            if (String (face->family_name).containsIgnoreCase (T("Sans"))
                                || String (face->family_name).containsIgnoreCase (T("Verdana"))
                                || String (face->family_name).containsIgnoreCase (T("Arial")))
                            {
                                newFace->setSerif (false);
                            }
                            else
                            {
                                newFace->setSerif (true);
                            }
                        }

                        FT_Done_Face (face);
                    }

                    ++faceIndex;
                }
                while (faceIndex < numFaces);
            }
        }
    }

    // Create a FreeType face object for a given font
    FT_Face createFT_Face (const String& fontName,
                           const bool bold,
                           const bool italic) throw()
    {
        FT_Face face = NULL;

        if (fontName == lastFontName && bold == lastBold && italic == lastItalic)
        {
            face = lastFace;
        }
        else
        {
            if (lastFace)
            {
                FT_Done_Face (lastFace);
                lastFace = NULL;
            }

            lastFontName = fontName;
            lastBold = bold;
            lastItalic = italic;

            FreeTypeFontFace* const ftFace = findOrCreate (fontName);

            if (ftFace != 0)
            {
                int style = (int) FreeTypeFontFace::Plain;

                if (bold)
                    style |= (int) FreeTypeFontFace::Bold;

                if (italic)
                    style |= (int) FreeTypeFontFace::Italic;

                int faceIndex;
                String fileName (ftFace->getFileName (style, &faceIndex));

                if (fileName.isEmpty())
                {
                    style ^= (int) FreeTypeFontFace::Bold;

                    fileName = ftFace->getFileName (style, &faceIndex);

                    if (fileName.isEmpty())
                    {
                        style ^= (int) FreeTypeFontFace::Bold;
                        style ^= (int) FreeTypeFontFace::Italic;

                        fileName = ftFace->getFileName (style, &faceIndex);

                        if (! fileName.length())
                        {
                            style ^= (int) FreeTypeFontFace::Bold;
                            fileName = ftFace->getFileName (style, &faceIndex);
                        }
                    }
                }

                if (! FT_New_Face (ftLib, (const char*) fileName, faceIndex, &lastFace))
                {
                    face = lastFace;

                    // If there isn't a unicode charmap then select the first one.
                    if (FT_Select_Charmap (face, ft_encoding_unicode))
                        FT_Set_Charmap (face, face->charmaps[0]);
                }
            }
        }
        return face;
    }

    void addGlyph (FT_Face face, Typeface& dest, uint32 character) throw()
    {
        const unsigned int glyphIndex = FT_Get_Char_Index (face, character);
        const float height = (float) (face->ascender - face->descender);
        const float scaleX = 1.0f / height;
        const float scaleY = -1.0f / height;
        Path destShape;

        #define CONVERTX(val) (scaleX * (val).x)
        #define CONVERTY(val) (scaleY * (val).y)

        if (FT_Load_Glyph (face, glyphIndex, FT_LOAD_NO_SCALE
                                              | FT_LOAD_NO_BITMAP
                                              | FT_LOAD_IGNORE_TRANSFORM) != 0
              || face->glyph->format != ft_glyph_format_outline)
        {
            return;
        }

        const FT_Outline* const outline = &face->glyph->outline;
        const short* const contours = outline->contours;
        const char* const tags = outline->tags;
        FT_Vector* const points = outline->points;

        for (int c = 0; c < outline->n_contours; c++)
        {
            const int startPoint = (c == 0) ? 0 : contours [c - 1] + 1;
            const int endPoint = contours[c];

            for (int p = startPoint; p <= endPoint; p++)
            {
                const float x = CONVERTX (points[p]);
                const float y = CONVERTY (points[p]);

                if (p == startPoint)
                {
                    destShape.startNewSubPath (x, y);
                }
                else if (FT_CURVE_TAG (tags[p]) == FT_Curve_Tag_On)
                {
                    destShape.lineTo (x, y);
                }
                else if (FT_CURVE_TAG (tags[p]) == FT_Curve_Tag_Conic)
                {
                    const int nextIndex = (p == endPoint) ? startPoint : p + 1;
                    float x2 = CONVERTX (points [nextIndex]);
                    float y2 = CONVERTY (points [nextIndex]);

                    if (FT_CURVE_TAG (tags [nextIndex]) == FT_Curve_Tag_Conic)
                    {
                        x2 = (x + x2) * 0.5f;
                        y2 = (y + y2) * 0.5f;
                    }
                    else
                    {
                        ++p;
                    }

                    destShape.quadraticTo (x, y, x2, y2);
                }
                else if (FT_CURVE_TAG (tags[p]) == FT_Curve_Tag_Cubic)
                {
                    if (p > endPoint-1)
                        return;

                    const int next1 = p + 1;
                    const int next2 = (p == (endPoint - 1)) ? startPoint : p + 2;

                    float x2 = CONVERTX (points [next1]);
                    float y2 = CONVERTY (points [next1]);
                    float x3 = CONVERTX (points [next2]);
                    float y3 = CONVERTY (points [next2]);

                    if (FT_CURVE_TAG(tags[next1]) != FT_Curve_Tag_Cubic)
                        return;

                    if (FT_CURVE_TAG(tags[next2]) != FT_Curve_Tag_On)
                        return;

                    destShape.cubicTo (x, y, x2, y2, x3, y3);
                    p += 2;
                }
            }

            destShape.closeSubPath();
        }

        dest.addGlyph (character, destShape, face->glyph->metrics.horiAdvance/height);

        if ((face->face_flags & FT_FACE_FLAG_KERNING) != 0)
            addKerning (face, dest, character, glyphIndex);
    }

    void addKerning (FT_Face face, Typeface& dest, const uint32 character, const uint32 glyphIndex) throw()
    {
        const float height = (float) (face->ascender - face->descender);

        uint32 rightGlyphIndex;
        uint32 rightCharCode = FT_Get_First_Char (face, &rightGlyphIndex);

        while (rightGlyphIndex != 0)
        {
            FT_Vector kerning;

            if (FT_Get_Kerning (face, glyphIndex, rightGlyphIndex, ft_kerning_unscaled, &kerning) == 0)
            {
                if (kerning.x != 0)
                    dest.addKerningPair (character, rightCharCode, kerning.x / height);
            }

            rightCharCode = FT_Get_Next_Char (face, rightCharCode, &rightGlyphIndex);
        }
    }

    // Add a glyph to a font
    bool addGlyphToFont (const uint32 character,
                         const tchar* fontName, bool bold, bool italic,
                         Typeface& dest) throw()
    {
        FT_Face face = createFT_Face (fontName, bold, italic);

        if (face != 0)
        {
            addGlyph (face, dest, character);
            return true;
        }

        return false;
    }

    // Create a Typeface object for given name/style
    bool createTypeface (const String& fontName,
                         const bool bold, const bool italic,
                         Typeface& dest,
                         const bool addAllGlyphs) throw()
    {
        dest.clear();
        dest.setName (fontName);
        dest.setBold (bold);
        dest.setItalic (italic);

        FT_Face face = createFT_Face (fontName, bold, italic);

        if (face == 0)
        {
#ifdef JUCE_DEBUG
            String msg (T("Failed to create typeface: "));
            msg << fontName << " " << (bold ? 'B' : ' ') << (italic ? 'I' : ' ');
            DBG (msg);
#endif
            return face;
        }

        const float height = (float) (face->ascender - face->descender);

        dest.setAscent (face->ascender / height);
        dest.setDefaultCharacter (L' ');

        if (addAllGlyphs)
        {
            uint32 glyphIndex;
            uint32 charCode = FT_Get_First_Char (face, &glyphIndex);

            while (glyphIndex != 0)
            {
                addGlyph (face, dest, charCode);
                charCode = FT_Get_Next_Char (face, charCode, &glyphIndex);
            }
        }

        return true;
    }

    //==============================================================================
    void getFamilyNames (StringArray& familyNames) const throw()
    {
        for (int i = 0; i < faces.size(); i++)
            familyNames.add (faces[i]->getFamilyName());
    }

    void getMonospacedNames (StringArray& monoSpaced) const throw()
    {
        for (int i = 0; i < faces.size(); i++)
            if (faces[i]->getMonospaced())
                monoSpaced.add (faces[i]->getFamilyName());
    }

    void getSerifNames (StringArray& serif) const throw()
    {
        for (int i = 0; i < faces.size(); i++)
            if (faces[i]->getSerif())
                serif.add (faces[i]->getFamilyName());
    }

    void getSansSerifNames (StringArray& sansSerif) const throw()
    {
        for (int i = 0; i < faces.size(); i++)
            if (! faces[i]->getSerif())
                sansSerif.add (faces[i]->getFamilyName());
    }

    juce_DeclareSingleton_SingleThreaded_Minimal (FreeTypeInterface)

private:
    //==============================================================================
    FT_Library ftLib;
    FT_Face lastFace;
    String lastFontName;
    bool lastBold, lastItalic;
    OwnedArray<FreeTypeFontFace> faces;
};

juce_ImplementSingleton_SingleThreaded (FreeTypeInterface)


//==============================================================================
void Typeface::initialiseTypefaceCharacteristics (const String& fontName,
                                                  bool bold, bool italic,
                                                  bool addAllGlyphsToFont) throw()
{
    FreeTypeInterface::getInstance()
        ->createTypeface (fontName, bold, italic, *this, addAllGlyphsToFont);
}

void Typeface::findAndAddSystemGlyph (juce_wchar character) throw()
{
    FreeTypeInterface::getInstance()
        ->addGlyphToFont (character, getName(), isBold(), isItalic(), *this);
}

const StringArray Font::findAllTypefaceNames() throw()
{
    StringArray s;
    FreeTypeInterface::getInstance()->getFamilyNames (s);
    s.sort (true);
    return s;
}

static const String pickBestFont (const StringArray& names,
                                  const char* const choicesString)
{
    StringArray choices;
    choices.addTokens (String (choicesString), T(","), 0);
    choices.trim();
    choices.removeEmptyStrings();

    int i, j;
    for (j = 0; j < choices.size(); ++j)
        if (names.contains (choices[j], true))
            return choices[j];

    for (j = 0; j < choices.size(); ++j)
        for (i = 0; i < names.size(); i++)
            if (names[i].startsWithIgnoreCase (choices[j]))
                return names[i];

    for (j = 0; j < choices.size(); ++j)
        for (i = 0; i < names.size(); i++)
            if (names[i].containsIgnoreCase (choices[j]))
                return names[i];

    return names[0];
}

static const String linux_getDefaultSansSerifFontName()
{
    StringArray allFonts;
    FreeTypeInterface::getInstance()->getSansSerifNames (allFonts);

    return pickBestFont (allFonts, "Verdana, Bitstream Vera Sans, Luxi Sans, Sans");
}

static const String linux_getDefaultSerifFontName()
{
    StringArray allFonts;
    FreeTypeInterface::getInstance()->getSerifNames (allFonts);

    return pickBestFont (allFonts, "Bitstream Vera Serif, Times, Nimbus Roman, Serif");
}

static const String linux_getDefaultMonospacedFontName()
{
    StringArray allFonts;
    FreeTypeInterface::getInstance()->getMonospacedNames (allFonts);

    return pickBestFont (allFonts, "Bitstream Vera Sans Mono, Courier, Sans Mono, Mono");
}

void Font::getDefaultFontNames (String& defaultSans, String& defaultSerif, String& defaultFixed) throw()
{
    defaultSans = linux_getDefaultSansSerifFontName();
    defaultSerif = linux_getDefaultSerifFontName();
    defaultFixed = linux_getDefaultMonospacedFontName();
}

END_JUCE_NAMESPACE

#endif
