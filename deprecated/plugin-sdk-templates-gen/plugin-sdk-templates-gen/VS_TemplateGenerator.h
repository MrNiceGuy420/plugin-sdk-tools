#pragma once
#define QUAZIP_STATIC
#include "VsProjectItemFile.h"
#include "ErrorMessage.h"
#include "JlCompress.h"

struct VS_ProjectGenerationSettings {
    QString vsDocumentsPath;
    QString pluginSdkPath;
    QString d3d9SdkPath;
    QString rwd3d9Path;
    QString vcCleoSdkPath;
    QString saCleoSdkPath;
    QString iiiCleoSdkPath;
    QString vcAsiOutputPath;
    QString saAsiOutputPath;
    QString iiiAsiOutputPath;
    QString vcCleoOutputPath;
    QString saCleoOutputPath;
    QString iiiCleoOutputPath;
};

enum eGtaGame {
    GTA_GAME_SA,
    GTA_GAME_VC,
    GTA_GAME_III
};

enum eGenerationFlags {
    VSGEN_D3D9 = 1,
    VSGEN_CLEO = 2,
    VSGEN_LA   = 4
};

class VS_ProjectTemplateGenerator {
public:
    static bool GenerateTemplate(QString fileName, int vsVersion, eGtaGame game, unsigned int flags, VS_ProjectGenerationSettings const &settings) {
        if (ProcessMainCppFile(game) && ProcessVSTemplateFile(game, vsVersion, flags) && ProcessVSProjectFile(game, vsVersion, flags, settings)) {
            QStringList fileNames;
            fileNames.append("templates\\temp\\Main.cpp");
            fileNames.append("templates\\temp\\Project.vcxproj");
            fileNames.append("templates\\temp\\MyTemplate.vstemplate");
            fileNames.append("templates\\vs" + QString::number(vsVersion) + "\\Project.vcxproj.filters");
            if(game == GTA_GAME_SA)
                fileNames.append("templates\\SA.ico");
            else if(game == GTA_GAME_VC)
                fileNames.append("templates\\VC.ico");
            else
                fileNames.append("templates\\III.ico");
            QString documentsPath = settings.vsDocumentsPath;
            if (!documentsPath.endsWith("vs" + QString::number(vsVersion) + '\\'))
                documentsPath += settings.vsDocumentsPath + "Templates\\ProjectTemplates\\Plugin-SDK\\"; 
            QString subFolder = (flags & VSGEN_CLEO) ? "CLEO\\" : "ASI\\";
            QString filePath = documentsPath + subFolder + fileName + ".zip";
            if (JlCompress::compressFiles(filePath, fileNames))
                return true;
            else
                return MESSAGE_ERROR("VS_ProjectTemplateGenerator::GenerateTemplate(): can't create .zip archive (" + filePath + ")");
        }
        return false;
    }

    static bool ProcessMainCppFile(eGtaGame game) {
        VsProjectItemFile mainCppFile;
        if (!mainCppFile.Read("templates\\Main.cpp"))
            return false;
        if (game == GTA_GAME_SA)
            mainCppFile.SetLinesValue("#include \"plugin.h\"", "#include \"plugin.h\"");
        else if (game == GTA_GAME_VC)
            mainCppFile.SetLinesValue("#include \"plugin.h\"", "#include \"plugin_vc.h\"");
        else
            mainCppFile.SetLinesValue("#include \"plugin.h\"", "#include \"plugin_III.h\"");
        return mainCppFile.Write("templates\\temp\\Main.cpp");
    }

    static bool ProcessVSTemplateFile(eGtaGame game, int vsVersion, unsigned int genFlags) {
        VsProjectItemFile vsTemplateFile;
        if (!vsTemplateFile.Read("templates\\vs" + QString::number(vsVersion) + "\\MyTemplate.vstemplate"))
            return false;
        QString name, description, icon;
        if (game == GTA_GAME_SA) {
            name = "GTA SA ";
            description = "A project for GTA San Andreas ";
            icon = "SA.ico";
        }
        else if (game == GTA_GAME_VC) {
            name = "GTA VC ";
            description = "A project for GTA Vice City ";
            icon = "VC.ico";
        }
        else {
            name = "GTA III ";
            description = "A project for GTA III ";
            icon = "III.ico";
        }
        if (genFlags & VSGEN_CLEO) {
            name += "CLEO plugin";
            description += "CLEO plugin";
        }
        else {
            name += ".ASI plugin";
            description += ".ASI plugin";
        }
        if (game == GTA_GAME_SA) {
            if (genFlags & VSGEN_D3D9) {
                name += " (D3D9";
                description += " with D3D9";
                if (genFlags & VSGEN_LA) {
                    name += "+LA comp.";
                    description += " and Limit Adjuster compatibility";
                }
                name += ')';
                description += ')';
            }
            else if (genFlags & VSGEN_LA) {
                name += " (+LA comp.)";
                description += " with Limit Adjuster compatibility";
            }
        }
        else {
            if (genFlags & VSGEN_D3D9) {
                name += " (D3D8toD3D9";
                description += " with D3D8toD3D9";
                if (genFlags & VSGEN_LA) {
                    name += "+LA comp.";
                    description += " and Limit Adjuster compatibility";
                }
                name += ')';
                description += ')';
            }
            else if (genFlags & VSGEN_LA) {
                name += " (+LA comp.)";
                description += " with Limit Adjuster compatibility";
            }
        }
        vsTemplateFile.SetNodesValue("Name", name);
        vsTemplateFile.SetNodesValue("Description", description);
        vsTemplateFile.SetNodesValue("Icon", icon);
        return vsTemplateFile.Write("templates\\temp\\MyTemplate.vstemplate");
    }

    static void AddValueToCSVLine(QString &line, QString value) {
        if (line.isEmpty())
            line = value;
        else
            line += ";" + value;
    }

    static QString LibraryName(int vsVersion, QString const &name) {
        if (vsVersion >= 2015)
            return name + ".lib";
        return name;
    }

    static QString ToCSV(QString value, bool atBegin) {
        if (value.isEmpty())
            return "";
        else {
            if (atBegin)
                return ";" + value;
            else
                return value + ";";
        }
    }

    static bool ProcessVSProjectFile(eGtaGame game, int vsVersion, unsigned int genFlags, VS_ProjectGenerationSettings const &settings) {
        VsProjectItemFile vsProjectFile;
        if (!vsProjectFile.Read("templates\\vs" + QString::number(vsVersion) + "\\Project.vcxproj"))
            return false;
        QString extension, outDir, definitions, includeFolders, libFolders, dependencies, vcIncludes, vcLibraries;

        QString cppStd = "c++14";

        if (vsVersion >= 2015) {
            AddValueToCSVLine(libFolders, settings.pluginSdkPath + "output\\lib\\");
            AddValueToCSVLine(definitions, "_USING_V110_SDK71_");
            AddValueToCSVLine(definitions, "_CRT_SECURE_NO_WARNINGS");
            AddValueToCSVLine(definitions, "_CRT_NON_CONFORMING_SWPRINTFS");
        }
        else
            AddValueToCSVLine(libFolders, settings.pluginSdkPath + "output\\mingw\\lib\\");
        
        if (genFlags & VSGEN_LA)
            AddValueToCSVLine(definitions, "_PLUGIN_LA_COMP");

        if (game == GTA_GAME_SA) {
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_sa\\");
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_sa\\game_sa\\");
            if (genFlags & VSGEN_CLEO)
                outDir = settings.saCleoOutputPath;
            else
                outDir = settings.saAsiOutputPath;
            AddValueToCSVLine(definitions, "GTASA");
            AddValueToCSVLine(definitions, "GTAGAME_NAME=\"San Andreas\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBR=\"SA\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBRLOW=\"sa\"");
            AddValueToCSVLine(definitions, "GTAGAME_PROTAGONISTNAME=\"CJ\"");
            AddValueToCSVLine(definitions, "GTAGAME_CITYNAME=\"San Andreas\"");
        }
        else if (game == GTA_GAME_VC) {
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_vc\\");
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_vc\\game_vc\\");
            if (genFlags & VSGEN_CLEO)
                outDir = settings.vcCleoOutputPath;
            else
                outDir = settings.vcAsiOutputPath;
            AddValueToCSVLine(definitions, "GTAVC");
            AddValueToCSVLine(definitions, "GTAGAME_NAME=\"Vice City\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBR=\"VC\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBRLOW=\"vc\"");
            AddValueToCSVLine(definitions, "GTAGAME_PROTAGONISTNAME=\"Tommy\"");
            AddValueToCSVLine(definitions, "GTAGAME_CITYNAME=\"Vice City\"");
        }
        else {
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_III\\");
            AddValueToCSVLine(includeFolders, settings.pluginSdkPath + "plugin_III\\game_III\\");
            if (genFlags & VSGEN_CLEO)
                outDir = settings.iiiCleoOutputPath;
            else
                outDir = settings.iiiAsiOutputPath;
            AddValueToCSVLine(definitions, "GTA3");
            AddValueToCSVLine(definitions, "GTAGAME_NAME=\"3\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBR=\"3\"");
            AddValueToCSVLine(definitions, "GTAGAME_ABBRLOW=\"3\"");
            AddValueToCSVLine(definitions, "GTAGAME_PROTAGONISTNAME=\"Claude\"");
            AddValueToCSVLine(definitions, "GTAGAME_CITYNAME=\"Liberty City\"");
        }

        if (genFlags & VSGEN_D3D9) {
            AddValueToCSVLine(vcIncludes, settings.d3d9SdkPath + "Include\\");
            AddValueToCSVLine(vcLibraries, settings.d3d9SdkPath + "Lib\\x86\\");
            AddValueToCSVLine(dependencies, LibraryName(vsVersion, "d3d9"));
            AddValueToCSVLine(dependencies, LibraryName(vsVersion, "d3dx9"));
            if (game != GTA_GAME_SA) {
                AddValueToCSVLine(dependencies, LibraryName(vsVersion, "rwd3d9"));
                AddValueToCSVLine(includeFolders, settings.rwd3d9Path + "source\\");
                AddValueToCSVLine(libFolders, settings.rwd3d9Path + "libs\\");
            }
            AddValueToCSVLine(definitions, "_DX9_SDK_INSTALLED");
        }

        if (genFlags & VSGEN_CLEO) {
            extension = ".cleo";
            if (game == GTA_GAME_SA) {
                AddValueToCSVLine(includeFolders, settings.saCleoSdkPath);
                AddValueToCSVLine(libFolders, settings.saCleoSdkPath);
                AddValueToCSVLine(dependencies, LibraryName(vsVersion, "cleo"));
            }
            else if (game == GTA_GAME_VC) {
                AddValueToCSVLine(includeFolders, settings.vcCleoSdkPath);
                AddValueToCSVLine(libFolders, settings.vcCleoSdkPath);
                AddValueToCSVLine(dependencies, LibraryName(vsVersion, "VC.CLEO"));
            }
            else {
                AddValueToCSVLine(includeFolders, settings.iiiCleoSdkPath);
                AddValueToCSVLine(libFolders, settings.iiiCleoSdkPath);
                AddValueToCSVLine(dependencies, LibraryName(vsVersion, "III.CLEO"));
            }
        }
        else
            extension = ".asi";

        QString pluginLibName = "plugin";
        if (game == GTA_GAME_VC)
            pluginLibName += "_vc";
        else if (game == GTA_GAME_III)
            pluginLibName += "_III";

        vsProjectFile.SetNodesValue("OutDir", outDir);

        vsProjectFile.SetNodesValue("NMakeOutput", "$TargetNameRelease$", true, "$(ProjectName)" + extension);
        vsProjectFile.SetNodesValue("NMakeOutput", "$TargetNameDebug$", true, "$(ProjectName)_d" + extension);
        vsProjectFile.SetNodesValue("NMakePreprocessorDefinitions", "$PreprocessorDefinitionsRelease$", true,
            definitions + ";NDEBUG;$(NMakePreprocessorDefinitions)");
        vsProjectFile.SetNodesValue("NMakePreprocessorDefinitions", "$PreprocessorDefinitionsDebug$", true,
            definitions + ";_DEBUG;$(NMakePreprocessorDefinitions)");
        if (!vcIncludes.isEmpty())
            vsProjectFile.SetNodesValue("NMakeIncludeSearchPath", vcIncludes + ";" + includeFolders + ";$(NMakeIncludeSearchPath)");
        else
            vsProjectFile.SetNodesValue("NMakeIncludeSearchPath", includeFolders + ";$(NMakeIncludeSearchPath)");
        QString nmToolCmd = "\"$(PLUGIN_SDK_DIR)\\tools\\general\\pluginsdk-build.exe\" ";
        QString nmBuildConfig = "buildtype:(DLL) ";
        QString nmProjConfig = "projectdir:\"($(ProjectDir))\" projectname:\"($(ProjectName))\" ";
        QString nmOutDirsRelease = "outdir:\"(" + outDir + ")\" intdir:\"($(ProjectDir).obj\\Release\\)\" ";
        QString nmOutDirsDebug = "outdir:\"(" + outDir + ")\" intdir:\"($(ProjectDir).obj\\Debug\\)\" ";
        QString nmTargetNameRelease = "targetname:\"($(ProjectName)" + extension + ")\" ";
        QString nmTargetNameDebug = "targetname:\"($(ProjectName)_d" + extension + ")\" ";
        QString allIncludeDirs;
        AddValueToCSVLine(allIncludeDirs, vcIncludes);
        AddValueToCSVLine(allIncludeDirs, "$(ProjectDir)");
        AddValueToCSVLine(allIncludeDirs, includeFolders);
        QString nmIncludeDirs = "includeDirs:\"(" + allIncludeDirs + ")\" ";
        QString allLibraryDirs;
        AddValueToCSVLine(allLibraryDirs, vcLibraries);
        AddValueToCSVLine(allLibraryDirs, libFolders);
        QString nmLibraryDirs = "libraryDirs:\"(" + allLibraryDirs + ")\" ";
        QString nmLibrariesRelease = "libraries:\"(paths;" + pluginLibName + ToCSV(dependencies, true) + ")\" ";
        QString nmLibrariesDebug = "libraries:\"(paths_d;" + pluginLibName + "_d" + ToCSV(dependencies, true) + ")\" ";
        QString nmDefinitionsRelease = "definitions:\"(" + definitions.replace("\"", "&lt;&gt;") + ";NDEBUG)\" ";
        QString nmDefinitionsDebug = "definitions:\"(" + definitions.replace("\"", "&lt;&gt;") + ";_DEBUG)\" ";
        QString nmAdditionalRelease = "additional:\"(-std=" + cppStd + " -m32 -O2 -fpermissive)\" ";
        QString nmAdditionalDebug = "additional:\"(-std=" + cppStd + " -m32 -g -fpermissive)\" ";
        QString nmLinkAdditionalRelease = "linkadditional:\"(-s -static-libgcc -static-libstdc++)\"";
        QString nmLinkAdditionalDebug = "linkadditional:\"(-static-libgcc -static-libstdc++)\"";
        QString nmDirsAndOptionsRelease = nmIncludeDirs + nmLibraryDirs + nmLibrariesRelease + nmDefinitionsRelease + nmAdditionalRelease + nmLinkAdditionalRelease;
        QString nmDirsAndOptionsDebug = nmIncludeDirs + nmLibraryDirs + nmLibrariesDebug + nmDefinitionsDebug + nmAdditionalDebug + nmLinkAdditionalDebug;

        vsProjectFile.SetNodesValue("NMakeBuildCommandLine", "$BuildCommandLineRelease$", true,
            nmToolCmd + "build " + nmBuildConfig + nmProjConfig + nmTargetNameRelease + nmOutDirsRelease + nmDirsAndOptionsRelease);
        vsProjectFile.SetNodesValue("NMakeReBuildCommandLine", "$RebuildCommandLineRelease$", true,
            nmToolCmd + "rebuild " + nmBuildConfig + nmProjConfig + nmTargetNameRelease + nmOutDirsRelease + nmDirsAndOptionsRelease);
        vsProjectFile.SetNodesValue("NMakeCleanCommandLine", "$CleanCommandLineRelease$", true,
            nmToolCmd + "clean " + nmProjConfig + nmTargetNameRelease + nmOutDirsRelease);

        vsProjectFile.SetNodesValue("NMakeBuildCommandLine", "$BuildCommandLineDebug$", true,
            nmToolCmd + "build " + nmBuildConfig + nmProjConfig + nmTargetNameDebug + nmOutDirsDebug + nmDirsAndOptionsDebug);
        vsProjectFile.SetNodesValue("NMakeReBuildCommandLine", "$RebuildCommandLineDebug$", true,
            nmToolCmd + "rebuild " + nmBuildConfig + nmProjConfig + nmTargetNameDebug + nmOutDirsDebug + nmDirsAndOptionsDebug);
        vsProjectFile.SetNodesValue("NMakeCleanCommandLine", "$CleanCommandLineDebug$", true,
            nmToolCmd + "clean " + nmProjConfig + nmTargetNameDebug + nmOutDirsDebug);

        return vsProjectFile.Write("templates\\temp\\Project.vcxproj");
    }
};
