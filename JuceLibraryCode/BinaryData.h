/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   FONTS_README_md;
    const int            FONTS_README_mdSize = 1927;

    extern const char*   Gyeol_Roadmap_md;
    const int            Gyeol_Roadmap_mdSize = 51941;

    extern const char*   NunitoRegular_ttf;
    const int            NunitoRegular_ttfSize = 132200;

    extern const char*   NunitoBold_ttf;
    const int            NunitoBold_ttfSize = 132148;

    extern const char*   NanumSquareRoundR_ttf;
    const int            NanumSquareRoundR_ttfSize = 1063276;

    extern const char*   NanumSquareRoundB_ttf;
    const int            NanumSquareRoundB_ttfSize = 1030948;

    extern const char*   NanumSquareRoundL_ttf;
    const int            NanumSquareRoundL_ttfSize = 1015112;

    extern const char*   NanumSquareRoundEB_ttf;
    const int            NanumSquareRoundEB_ttfSize = 1029388;

    extern const char*   Teul_Roadmap_md;
    const int            Teul_Roadmap_mdSize = 32595;

    extern const char*   Teul_UI_Roadmap_md;
    const int            Teul_UI_Roadmap_mdSize = 27409;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 10;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
