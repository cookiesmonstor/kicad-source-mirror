/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004-2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2011 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 2016-2020 KiCad Developers, see AUTHORS.txt for contributors.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <confirm.h>
#include <kicad_string.h>
#include <gestfich.h>
#include <pcb_edit_frame.h>
#include <3d_viewer/eda_3d_viewer.h>
#include <pgm_base.h>
#include <msgpanel.h>
#include <fp_lib_table.h>
#include <kiface_i.h>
#include <trace_helpers.h>
#include <lockfile.cpp>
#include <netlist_reader/pcb_netlist.h>
#include <pcbnew_id.h>
#include <io_mgr.h>
#include <wildcards_and_files_ext.h>
#include <tool/tool_manager.h>
#include <class_board.h>
#include <wx/stdpaths.h>
#include <ratsnest/ratsnest_data.h>
#include <kiplatform/app.h>
#include <widgets/appearance_controls.h>
#include <wx/wupdlock.h>
#include <settings/common_settings.h>
#include <settings/settings_manager.h>
#include <project/project_file.h>
#include <project/project_local_settings.h>
#include <plugins/cadstar/cadstar_pcb_archive_plugin.h>
#include <dialogs/dialog_imported_layers.h>


//#define     USE_INSTRUMENTATION     1
#define     USE_INSTRUMENTATION     0


/**
 * Function AskLoadBoardFileName
 * puts up a wxFileDialog asking for a BOARD filename to open.
 *
 * @param aParent is a wxFrame passed to wxFileDialog.
 * @param aCtl is where to put the OpenProjectFiles() control bits.
 *
 * @param aFileName on entry is a probable choice, on return is the chosen filename.
 * @param aKicadFilesOnly true to list kiacad pcb files plugins only, false to list import plugins.
 *
 * @return bool - true if chosen, else false if user aborted.
 */
bool AskLoadBoardFileName( wxWindow* aParent, int* aCtl, wxString* aFileName, bool aKicadFilesOnly )
{
    // This is a subset of all PLUGINs which are trusted to be able to
    // load a BOARD. User may occasionally use the wrong plugin to load a
    // *.brd file (since both legacy and eagle use *.brd extension),
    // but eventually *.kicad_pcb will be more common than legacy *.brd files.

    // clang-format off
    static const struct
    {
        const wxString&     filter;
        IO_MGR::PCB_FILE_T  pluginType;
    } loaders[] =
    {
        { PcbFileWildcard(),                    IO_MGR::KICAD_SEXP },            // Current Kicad board files
        { LegacyPcbFileWildcard(),              IO_MGR::LEGACY },                // Old Kicad board files
        { AltiumCircuitMakerPcbFileWildcard(),  IO_MGR::ALTIUM_CIRCUIT_MAKER },  // Import Altium Circuit Maker board files
        { AltiumCircuitStudioPcbFileWildcard(), IO_MGR::ALTIUM_CIRCUIT_STUDIO }, // Import Altium Circuit Studio board files
        { AltiumDesignerPcbFileWildcard(),      IO_MGR::ALTIUM_DESIGNER },       // Import Altium Designer board files
        { CadstarPcbArchiveFileWildcard(),      IO_MGR::CADSTAR_PCB_ARCHIVE },   // Import Cadstar PCB Archive board files
        { EaglePcbFileWildcard(),               IO_MGR::EAGLE },                 // Import Eagle board files
        { PCadPcbFileWildcard(),                IO_MGR::PCAD }                   // Import PCAD board files
    };
    // clang-format on

    wxFileName  fileName( *aFileName );
    wxString    fileFilters;

    if( aKicadFilesOnly )
    {
        std::vector<std::string> fileExtensions;

        for( unsigned ii = 0; ii < 2; ++ii )
        {
            if( !fileFilters.IsEmpty() )
                fileFilters += wxChar( '|' );

            fileFilters += wxGetTranslation( loaders[ii].filter );

            PLUGIN::RELEASER plugin( IO_MGR::PluginFind( loaders[ii].pluginType ) );
            wxCHECK( plugin, false );
            fileExtensions.push_back( plugin->GetFileExtension().ToStdString() );
        }

        fileFilters = _( "All KiCad Board Files" ) + AddFileExtListToFilter( fileExtensions ) + "|"
                      + fileFilters;
    }
    else
    {
        wxString allWildcards;

        for( unsigned ii = 2; ii < arrayDim( loaders ); ++ii )
        {
            if( !fileFilters.IsEmpty() )
                fileFilters += wxChar( '|' );

            fileFilters += wxGetTranslation( loaders[ii].filter );

            PLUGIN::RELEASER plugin( IO_MGR::PluginFind( loaders[ii].pluginType ) );
            wxCHECK( plugin, false );
            allWildcards += "*." + formatWildcardExt( plugin->GetFileExtension() ) + ";";
        }

        fileFilters = _( "All supported formats|" ) + allWildcards + "|" + fileFilters;
    }


    wxString    path;
    wxString    name;

    if( fileName.FileExists() )
    {
        path = fileName.GetPath();
        name = fileName.GetFullName();
    }
    else
    {
        path = wxStandardPaths::Get().GetDocumentsDir();
        // leave name empty
    }

    wxFileDialog dlg( aParent,
                      aKicadFilesOnly ? _( "Open Board File" ) : _( "Import Non KiCad Board File" ),
                      path, name, fileFilters,
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST );

    if( dlg.ShowModal() == wxID_OK )
    {
        // For import option, if Eagle (*.brd files), tell OpenProjectFiles() to use Eagle plugin.
        // It's the only special case because of the duplicate use of the *.brd file extension.
        // Other cases are clear because of unique file extensions.
        *aCtl = aKicadFilesOnly ? 0 : KICTL_EAGLE_BRD;
        *aFileName = dlg.GetPath();
        return true;
    }
    else
        return false;
}


///> Helper widget to select whether a new project should be created for a file when saving
class CREATE_PROJECT_CHECKBOX : public wxPanel
{
public:
    CREATE_PROJECT_CHECKBOX( wxWindow* aParent )
            : wxPanel( aParent )
    {
        m_cbCreateProject = new wxCheckBox( this, wxID_ANY,
                                            _( "Create a new project for this board" ) );
        m_cbCreateProject->SetValue( false );
        m_cbCreateProject->SetToolTip( _( "Creating a project will enable features such as "
                                          "design rules, net classes, and layer presets" ) );

        wxBoxSizer* sizer = new wxBoxSizer( wxHORIZONTAL );
        sizer->Add( m_cbCreateProject, 0, wxALL, 8 );

        SetSizerAndFit( sizer );
    }

    bool GetValue() const
    {
        return m_cbCreateProject->GetValue();
    }

    static wxWindow* Create( wxWindow* aParent )
    {
        return new CREATE_PROJECT_CHECKBOX( aParent );
    }

protected:
    wxCheckBox* m_cbCreateProject;
};


/**
 * Puts up a wxFileDialog asking for a BOARD filename to save.
 *
 * @param aParent is a wxFrame passed to wxFileDialog.
 * @param aFileName on entry is a probable choice, on return is the
 *  chosen full filename (includes path).
 * @param aCreateProject will be filled with the state of the Create Project? checkbox if relevant
 *
 * @return bool - true if chosen, else false if user aborted.
 */
bool AskSaveBoardFileName( PCB_EDIT_FRAME* aParent, wxString* aFileName, bool* aCreateProject )
{
    wxString    wildcard =  PcbFileWildcard();
    wxFileName  fn = *aFileName;

    fn.SetExt( KiCadPcbFileExtension );

    wxFileDialog dlg( aParent,
            _( "Save Board File As" ),
            fn.GetPath(),
            fn.GetFullName(),
            wildcard,
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT
            );

    // Add a "Create a project" checkbox in standalone mode and one isn't loaded
    if( Kiface().IsSingle() && aParent->Prj().IsNullProject() )
        dlg.SetExtraControlCreator( &CREATE_PROJECT_CHECKBOX::Create );

    if( dlg.ShowModal() != wxID_OK )
        return false;

    fn = dlg.GetPath();

    // always enforce filename extension, user may not have entered it.
    fn.SetExt( KiCadPcbFileExtension );

    *aFileName = fn.GetFullPath();

    if( wxWindow* extraControl = dlg.GetExtraControl() )
        *aCreateProject = static_cast<CREATE_PROJECT_CHECKBOX*>( extraControl )->GetValue();

    return true;
}


void PCB_EDIT_FRAME::OnFileHistory( wxCommandEvent& event )
{
    wxString fn = GetFileFromHistory( event.GetId(), _( "Printed circuit board" ) );

    if( !!fn )
    {
        int open_ctl = 0;

        if( !wxFileName::IsFileReadable( fn ) )
        {
            if( !AskLoadBoardFileName( this, &open_ctl, &fn, true ) )
                return;
        }

        OpenProjectFiles( std::vector<wxString>( 1, fn ), open_ctl );
    }
}


void PCB_EDIT_FRAME::OnClearFileHistory( wxCommandEvent& aEvent )
{
    ClearFileHistory();
}


void PCB_EDIT_FRAME::Files_io( wxCommandEvent& event )
{
    int        id = event.GetId();
    Files_io_from_id( id );
}


bool PCB_EDIT_FRAME::Files_io_from_id( int id )
{
    wxString   msg;

    switch( id )
    {
    case ID_LOAD_FILE:
        {
            int         open_ctl = 0;
            wxString    fileName = Prj().AbsolutePath( GetBoard()->GetFileName() );

            return AskLoadBoardFileName( this, &open_ctl, &fileName, true )
                       && OpenProjectFiles( std::vector<wxString>( 1, fileName ), open_ctl );
        }

    case ID_IMPORT_NON_KICAD_BOARD:
        {
            int         open_ctl = 1;
            wxString    fileName; // = Prj().AbsolutePath( GetBoard()->GetFileName() );

            return AskLoadBoardFileName( this, &open_ctl, &fileName, false )
                       && OpenProjectFiles( std::vector<wxString>( 1, fileName ), open_ctl );
        }

    case ID_MENU_RECOVER_BOARD_AUTOSAVE:
        {
            wxFileName currfn = Prj().AbsolutePath( GetBoard()->GetFileName() );
            wxFileName fn = currfn;

            wxString rec_name = GetAutoSaveFilePrefix() + fn.GetName();
            fn.SetName( rec_name );

            if( !fn.FileExists() )
            {
                msg.Printf( _( "Recovery file \"%s\" not found." ), fn.GetFullPath() );
                DisplayInfoMessage( this, msg );
                return false;
            }

            msg.Printf( _( "OK to load recovery file \"%s\"" ), fn.GetFullPath() );

            if( !IsOK( this, msg ) )
                return false;

            GetScreen()->ClrModify();    // do not prompt the user for changes

            if( OpenProjectFiles( std::vector<wxString>( 1, fn.GetFullPath() ) ) )
            {
                // Re-set the name since name or extension was changed
                GetBoard()->SetFileName( currfn.GetFullPath() );
                UpdateTitle();
                return true;
            }
            return false;
        }

    case ID_NEW_BOARD:
    {
        if( IsContentModified() )
        {
            wxFileName fileName = GetBoard()->GetFileName();
            wxString   saveMsg =
                    _( "Current board will be closed, save changes to \"%s\" before continuing?" );

            if( !HandleUnsavedChanges( this, wxString::Format( saveMsg, fileName.GetFullName() ),
                                       [&]()->bool { return Files_io_from_id( ID_SAVE_BOARD ); } ) )
                return false;
        }
        else if( !GetBoard()->IsEmpty() )
        {
            if( !IsOK( this, _( "Current Board will be closed. Continue?" ) ) )
                return false;
        }

        SaveProjectSettings();

        GetBoard()->ClearProject();

        SETTINGS_MANAGER* mgr = GetSettingsManager();

        mgr->SaveProject( mgr->Prj().GetProjectFullName() );
        mgr->UnloadProject( &mgr->Prj() );

        if( !Clear_Pcb( false ) )
            return false;

        onBoardLoaded();

        LoadProjectSettings();

        OnModify();
        return true;
    }

    case ID_SAVE_BOARD:
        if( !GetBoard()->GetFileName().IsEmpty() )
            return SavePcbFile( Prj().AbsolutePath( GetBoard()->GetFileName() ) );

        KI_FALLTHROUGH;

    case ID_COPY_BOARD_AS:
    case ID_SAVE_BOARD_AS:
        {
            bool addToHistory = false;
            wxString orig_name;
            wxFileName::SplitPath( GetBoard()->GetFileName(), nullptr, nullptr, &orig_name,
                                   nullptr );

            if( orig_name.IsEmpty() )
            {
                addToHistory = true;
                orig_name = _( "noname" );
            }

            wxString    pro_dir = wxPathOnly( Prj().GetProjectFullName() );
            wxFileName  fn( pro_dir, orig_name, KiCadPcbFileExtension );
            wxString    filename = fn.GetFullPath();

            bool createProject = false;

            if( AskSaveBoardFileName( this, &filename, &createProject ) )
            {
                if( id == ID_COPY_BOARD_AS )
                    return SavePcbCopy( filename, createProject );
                else
                    return SavePcbFile( filename, addToHistory, createProject );
            }
            return false;
        }

    default:
        return false;
    }
}


// The KIWAY_PLAYER::OpenProjectFiles() API knows nothing about plugins, so
// determine how to load the BOARD here, with minor assistance from KICTL_EAGLE_BRD
// bit flag.
IO_MGR::PCB_FILE_T plugin_type( const wxString& aFileName, int aCtl )
{
    IO_MGR::PCB_FILE_T  pluginType;

    wxFileName fn = aFileName;

    // Note: file extensions are expected to be in lower case.
    // This is not always true, especially when importing files, so the string
    // comparisons are case insensitive to try to find the suitable plugin.

    if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::LEGACY ) ) == 0 )
    {
        // both legacy and eagle share a common file extension.
        pluginType = ( aCtl & KICTL_EAGLE_BRD ) ? IO_MGR::EAGLE : IO_MGR::LEGACY;
    }
    else if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::PCAD ) ) == 0 )
    {
        pluginType = IO_MGR::PCAD;
    }
    else if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::ALTIUM_DESIGNER ) ) == 0 )
    {
        pluginType = IO_MGR::ALTIUM_DESIGNER;
    }
    else if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::ALTIUM_CIRCUIT_STUDIO ) ) == 0 )
    {
        pluginType = IO_MGR::ALTIUM_CIRCUIT_STUDIO;
    }
    else if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::ALTIUM_CIRCUIT_MAKER ) ) == 0 )
    {
        pluginType = IO_MGR::ALTIUM_CIRCUIT_MAKER;
    }
    else if( fn.GetExt().CmpNoCase( IO_MGR::GetFileExtension( IO_MGR::CADSTAR_PCB_ARCHIVE ) ) == 0 )
    {
        pluginType = IO_MGR::CADSTAR_PCB_ARCHIVE;
    }
    else
    {
        pluginType = IO_MGR::KICAD_SEXP;
    }

    return pluginType;
}


int PCB_EDIT_FRAME::inferLegacyEdgeClearance( BOARD* aBoard )
{
    PCB_LAYER_COLLECTOR collector;

    collector.SetLayerId( Edge_Cuts );
    collector.Collect( aBoard, GENERAL_COLLECTOR::AllBoardItems );

    int  edgeWidth = -1;
    bool mixed = false;

    for( int i = 0; i < collector.GetCount(); i++ )
    {
        if( collector[i]->Type() == PCB_SHAPE_T )
        {
            int itemWidth = static_cast<PCB_SHAPE*>( collector[i] )->GetWidth();

            if( edgeWidth != -1 && edgeWidth != itemWidth )
            {
                mixed = true;
                edgeWidth = std::max( edgeWidth, itemWidth );
            }
            else
            {
                edgeWidth = itemWidth;
            }
        }
    }

    if( mixed )
    {
        // If they had different widths then we can't ensure that fills will be the same.
        wxMessageBox( _( "If the zones on this board are refilled the Copper Edge Clearance\n"
                         "setting will be used (see Board Setup > Design Rules).  This may\n"
                         "result in different fills from previous Kicad versions which used\n"
                         "the line thickness of the board boundary on the Edge Cuts layer." ),
                      _( "Edge Clearance Warning" ), wxOK|wxICON_WARNING, this );
    }

    return std::max( 0, edgeWidth / 2 );
}


bool PCB_EDIT_FRAME::OpenProjectFiles( const std::vector<wxString>& aFileSet, int aCtl )
{
    // This is for python:
    if( aFileSet.size() != 1 )
    {
        UTF8 msg = StrPrintf( "Pcbnew:%s() takes only a single filename", __func__ );
        DisplayError( this, msg );
        return false;
    }

    wxString fullFileName( aFileSet[0] );

    if( Kiface().IsSingle() )
    {
        KIPLATFORM::APP::RegisterApplicationRestart( fullFileName );
    }

    // We insist on caller sending us an absolute path, if it does not, we say it's a bug.
    wxASSERT_MSG( wxFileName( fullFileName ).IsAbsolute(), wxT( "Path is not absolute!" ) );

    std::unique_ptr<wxSingleInstanceChecker> lockFile = ::LockFile( fullFileName );

    if( !lockFile )
    {
        wxString msg = wxString::Format( _( "PCB file \"%s\" is already open." ), fullFileName );
        DisplayError( this, msg );
        return false;
    }

    if( IsContentModified() )
    {
        if( !HandleUnsavedChanges( this, _( "The current PCB has been modified.  Save changes?" ),
            [&]()->bool { return SavePcbFile( GetBoard()->GetFileName() ); } ) )
        {
            return false;
        }
    }

    // Release the lock file, until the new file is actually loaded
    ReleaseFile();

    wxFileName pro = fullFileName;
    pro.SetExt( ProjectFileExtension );

    bool is_new = !wxFileName::IsFileReadable( fullFileName );

    // If its a non-existent schematic and caller thinks it exists
    if( is_new && !( aCtl & KICTL_CREATE ) )
    {
        // notify user that fullFileName does not exist, ask if user wants to create it.
        wxString ask = wxString::Format( _( "PCB \"%s\" does not exist.  Do you wish to create it?" ),
                                         fullFileName );
        if( !IsOK( this, ask ) )
            return false;
    }

    // Unlink the old project if needed
    GetBoard()->ClearProject();

    // No save prompt (we already prompted above), and only reset to a new blank board if new
    Clear_Pcb( false, !is_new );

    IO_MGR::PCB_FILE_T  pluginType = plugin_type( fullFileName, aCtl );

    bool converted =  pluginType != IO_MGR::LEGACY && pluginType != IO_MGR::KICAD_SEXP;

    if( !converted )
    {
        // Loading a project should only be done under carefully considered circumstances.

        // The calling code should know not to ask me here to change projects unless
        // it knows what consequences that will have on other KIFACEs running and using
        // this same PROJECT.  It can be very harmful if that calling code is stupid.
        SETTINGS_MANAGER* mgr = GetSettingsManager();

        if( pro.GetFullPath() != mgr->Prj().GetProjectFullName() )
        {
            // calls SaveProject
            SaveProjectSettings();

            mgr->UnloadProject( &mgr->Prj() );

            mgr->LoadProject( pro.GetFullPath() );

            // Do not allow saving a project if one doesn't exist.  This normally happens if we are
            // standalone and opening a board that has been moved from its project folder.
            if( !pro.Exists() )
                Prj().SetReadOnly();
        }
    }

    if( is_new )
    {
        // Link the existing blank board to the new project
        GetBoard()->SetProject( &Prj() );

        GetBoard()->SetFileName( fullFileName );

        OnModify();
    }
    else
    {
        BOARD* loadedBoard = 0;   // it will be set to non-NULL if loaded OK

        PLUGIN::RELEASER pi( IO_MGR::PluginFind( pluginType ) );


        if( pluginType == IO_MGR::CADSTAR_PCB_ARCHIVE )
        {
            // TODO: Generalise this so that it is applicable to all non-kicad plugins
            CADSTAR_PCB_ARCHIVE_PLUGIN* cadstarPlugin = nullptr;

            cadstarPlugin = dynamic_cast<CADSTAR_PCB_ARCHIVE_PLUGIN*>( (PLUGIN*) pi );

            wxCHECK( cadstarPlugin, false );

            cadstarPlugin->RegisterLayerMappingCallback(
                    std::bind( DIALOG_IMPORTED_LAYERS::GetMapModal, this, std::placeholders::_1 ) );
        }

        // This will rename the file if there is an autosave and the user want to recover
		CheckForAutoSaveFile( fullFileName );

        try
        {
            PROPERTIES  props;
            char        xbuf[30];
            char        ybuf[30];

            // EAGLE_PLUGIN can use this info to center the BOARD, but it does not yet.
            sprintf( xbuf, "%d", GetPageSizeIU().x );
            sprintf( ybuf, "%d", GetPageSizeIU().y );

            props["page_width"]  = xbuf;
            props["page_height"] = ybuf;

#if USE_INSTRUMENTATION
            // measure the time to load a BOARD.
            unsigned startTime = GetRunningMicroSecs();
#endif

            loadedBoard = pi->Load( fullFileName, NULL, &props );

#if USE_INSTRUMENTATION
            unsigned stopTime = GetRunningMicroSecs();
            printf( "PLUGIN::Load(): %u usecs\n", stopTime - startTime );
#endif
        }
        catch( const IO_ERROR& ioe )
        {
            if( ioe.Problem() != wxT( "CANCEL" ) )
            {
                wxString msg = wxString::Format( _( "Error loading board file:\n%s" ), fullFileName );
                DisplayErrorMessage( this, msg, ioe.What() );
            }

            // We didn't create a new blank board above, so do that now
            Clear_Pcb( false );

            return false;
        }

        SetBoard( loadedBoard );

        // On save; design settings will be removed from the board
        if( loadedBoard->m_LegacyDesignSettingsLoaded )
            loadedBoard->SetModified();

        // Move legacy view settings to local project settings
        if( !loadedBoard->m_LegacyVisibleLayers.test( Rescue ) )
        {
            Prj().GetLocalSettings().m_VisibleLayers = loadedBoard->m_LegacyVisibleLayers;
            loadedBoard->SetModified();
        }

        if( !loadedBoard->m_LegacyVisibleItems.test( GAL_LAYER_INDEX( GAL_LAYER_ID_BITMASK_END ) ) )
        {
            Prj().GetLocalSettings().m_VisibleItems = loadedBoard->m_LegacyVisibleItems;
            loadedBoard->SetModified();
        }

        // we should not ask PLUGINs to do these items:
        loadedBoard->BuildListOfNets();
        ResolveDRCExclusions();

        if( loadedBoard->IsModified() )
            OnModify();
        else
            GetScreen()->ClrModify();

        if( pluginType == IO_MGR::LEGACY &&
            loadedBoard->GetFileFormatVersionAtLoad() < LEGACY_BOARD_FILE_VERSION )
        {
            DisplayInfoMessage( this,
                _(  "This file was created by an older version of Pcbnew.\n"
                    "It will be stored in the new file format when you save this file again." ) );
        }
    }

    {
        wxFileName fn = fullFileName;

        if( converted )
            fn.SetExt( PcbFileExtension );

        wxString fname = fn.GetFullPath();

        fname.Replace( WIN_STRING_DIR_SEP, UNIX_STRING_DIR_SEP );

        GetBoard()->SetFileName( fname );
    }

    // Lock the file newly opened:
    m_file_checker.reset( lockFile.release() );

    if( !converted )
        UpdateFileHistory( GetBoard()->GetFileName() );

    // Rebuild list of nets (full ratsnest rebuild)
    GetBoard()->BuildConnectivity();
    Compile_Ratsnest( true );

    // Load project settings after setting up board; some of them depend on the nets list
    LoadProjectSettings();

    // Syncs the UI (appearance panel, etc) with the loaded board and project
    onBoardLoaded();

    // Refresh the 3D view, if any
    EDA_3D_VIEWER* draw3DFrame = Get3DViewerFrame();

    if( draw3DFrame )
        draw3DFrame->NewDisplay();

#if 0 && defined(DEBUG)
    // Output the board object tree to stdout, but please run from command prompt:
    GetBoard()->Show( 0, std::cout );
#endif

    // from EDA_APPL which was first loaded BOARD only:
    {
        /* For an obscure reason the focus is lost after loading a board file
         * when starting up the process.
         * (seems due to the recreation of the layer manager after loading the file)
         * Give focus to main window and Drawpanel
         * must be done for these 2 windows (for an obscure reason ...)
         * Linux specific
         * This is more a workaround than a fix.
         */
        SetFocus();
        GetCanvas()->SetFocus();
    }

    return true;
}


bool PCB_EDIT_FRAME::SavePcbFile( const wxString& aFileName, bool addToHistory,
                                  bool aChangeProject )
{
    // please, keep it simple.  prompting goes elsewhere.

    wxFileName pcbFileName = aFileName;

    if( pcbFileName.GetExt() == LegacyPcbFileExtension )
        pcbFileName.SetExt( KiCadPcbFileExtension );

    if( !IsWritable( pcbFileName ) )
    {
        wxString msg = wxString::Format( _(
            "No access rights to write to file \"%s\"" ),
            pcbFileName.GetFullPath() );

        DisplayError( this, msg );
        return false;
    }

    // TODO: this will break if we ever go multi-board
    wxFileName projectFile( pcbFileName );
    bool       projectFileExists = false;

    projectFile.SetExt( ProjectFileExtension );
    projectFileExists = projectFile.FileExists();

    if( aChangeProject && !projectFileExists )
    {
        // If this is a new board, project filename won't be set yet
        if( projectFile.GetFullPath() != Prj().GetProjectFullName() )
        {
            GetBoard()->ClearProject();

            SETTINGS_MANAGER* mgr = GetSettingsManager();

            mgr->SaveProject( Prj().GetProjectFullName() );
            mgr->UnloadProject( &Prj() );

            // If no project to load then initialize project text vars with board properties
            if( !mgr->LoadProject( projectFile.GetFullPath() ) )
                Prj().GetTextVars() = GetBoard()->GetProperties();

            GetBoard()->SetProject( &Prj() );
        }
    }

    if( projectFileExists )
        GetBoard()->SynchronizeProperties();

    wxFileName tempFile( aFileName );
    tempFile.SetName( wxT( "." ) + tempFile.GetName() );
    tempFile.SetExt( tempFile.GetExt() + wxT( "$" ) );

    GetBoard()->SynchronizeNetsAndNetClasses();

    // Save various DRC parameters, such as violation severities (which may have been
    // edited via the DRC dialog as well as the Board Setup dialog), DRC exclusions, etc.
    SaveProjectSettings();

    GetSettingsManager()->SaveProject();

    ClearMsgPanel();

    wxString    upperTxt;
    wxString    lowerTxt;

    try
    {
        PLUGIN::RELEASER    pi( IO_MGR::PluginFind( IO_MGR::KICAD_SEXP ) );

        wxASSERT( tempFile.IsAbsolute() );

        pi->Save( tempFile.GetFullPath(), GetBoard(), NULL );
    }
    catch( const IO_ERROR& ioe )
    {
        wxString msg = wxString::Format( _(
                "Error saving board file \"%s\".\n%s" ),
                pcbFileName.GetFullPath(), ioe.What()
                );
        DisplayError( this, msg );

        lowerTxt.Printf( _( "Failed to create temporary file \"%s\"" ), tempFile.GetFullPath() );

        AppendMsgPanel( upperTxt, lowerTxt, CYAN );

        // In case we started a file but didn't fully write it, clean up
        wxRemoveFile( tempFile.GetFullPath() );

        return false;
    }

    // If save succeeded, replace the original with what we just wrote
    if( !wxRenameFile( tempFile.GetFullPath(), pcbFileName.GetFullPath() ) )
    {
        wxString msg = wxString::Format( _(
                "Error saving board file \"%s\".\nFailed to rename temporary file \"%s\"" ),
                pcbFileName.GetFullPath(), tempFile.GetFullPath()
                );
        DisplayError( this, msg );

        lowerTxt.Printf( _( "Failed to rename temporary file \"%s\"" ), tempFile.GetFullPath() );

        AppendMsgPanel( upperTxt, lowerTxt, CYAN );

        return false;
    }

    if( !Kiface().IsSingle() )
    {
        WX_STRING_REPORTER backupReporter( &upperTxt );

        if( GetSettingsManager()->TriggerBackupIfNeeded( backupReporter ) )
            upperTxt.clear();
    }

    GetBoard()->SetFileName( pcbFileName.GetFullPath() );
    UpdateTitle();

    // Put the saved file in File History if requested
    if( addToHistory )
        UpdateFileHistory( GetBoard()->GetFileName() );

    // Delete auto save file on successful save.
    wxFileName autoSaveFileName = pcbFileName;

    autoSaveFileName.SetName( GetAutoSaveFilePrefix() + pcbFileName.GetName() );

    if( autoSaveFileName.FileExists() )
        wxRemoveFile( autoSaveFileName.GetFullPath() );

    lowerTxt.Printf( _( "Wrote board file: \"%s\"" ), pcbFileName.GetFullPath() );

    AppendMsgPanel( upperTxt, lowerTxt, CYAN );

    GetScreen()->ClrModify();
    GetScreen()->ClrSave();
    return true;
}


bool PCB_EDIT_FRAME::SavePcbCopy( const wxString& aFileName, bool aCreateProject )
{
    wxFileName  pcbFileName = aFileName;

    // Ensure the file ext is the right ext:
    pcbFileName.SetExt( KiCadPcbFileExtension );

    if( !IsWritable( pcbFileName ) )
    {
        wxString msg = wxString::Format( _(
            "No access rights to write to file \"%s\"" ),
            pcbFileName.GetFullPath() );

        DisplayError( this, msg );
        return false;
    }

    GetBoard()->SynchronizeNetsAndNetClasses();

    try
    {
        PLUGIN::RELEASER    pi( IO_MGR::PluginFind( IO_MGR::KICAD_SEXP ) );

        wxASSERT( pcbFileName.IsAbsolute() );

        pi->Save( pcbFileName.GetFullPath(), GetBoard(), NULL );
    }
    catch( const IO_ERROR& ioe )
    {
        wxString msg = wxString::Format( _(
                "Error saving board file \"%s\".\n%s" ),
                pcbFileName.GetFullPath(), ioe.What()
                );
        DisplayError( this, msg );

        return false;
    }

    if( aCreateProject )
    {
        wxFileName projectFile( pcbFileName );
        projectFile.SetExt( ProjectFileExtension );

        if( !projectFile.FileExists() )
        {
            wxString currentProject = Prj().GetProjectFullName();

            SETTINGS_MANAGER* mgr = GetSettingsManager();

            GetBoard()->ClearProject();

            mgr->SaveProject( currentProject );
            mgr->UnloadProject( &Prj() );

            mgr->LoadProject( projectFile.GetFullPath() );
            mgr->SaveProject();

            mgr->UnloadProject( &Prj() );
            mgr->LoadProject( currentProject );

            // If no project to load then initialize project text vars with board properties
            if( !mgr->LoadProject( currentProject ) )
                Prj().GetTextVars() = GetBoard()->GetProperties();

            GetBoard()->SetProject( &Prj() );
        }
    }

    DisplayInfoMessage( this, wxString::Format( _( "Board copied to:\n\"%s\"" ),
                                                pcbFileName.GetFullPath() ) );

    return true;
}


bool PCB_EDIT_FRAME::doAutoSave()
{
    wxFileName tmpFileName;

    if( GetBoard()->GetFileName().IsEmpty() )
    {
        tmpFileName = wxFileName( wxStandardPaths::Get().GetDocumentsDir(), wxT( "noname" ),
                                  KiCadPcbFileExtension );
        GetBoard()->SetFileName( tmpFileName.GetFullPath() );
    }
    else
    {
        tmpFileName = Prj().AbsolutePath( GetBoard()->GetFileName() );
    }

    wxFileName autoSaveFileName = tmpFileName;

    // Auto save file name is the board file name prepended with autosaveFilePrefix string.
    autoSaveFileName.SetName( GetAutoSaveFilePrefix() + autoSaveFileName.GetName() );

    if( !autoSaveFileName.IsOk() )
        return false;

    // If the board file path is not writable, try writing to a platform specific temp file
    // path.  If that path isn't writabe, give up.
    if( !autoSaveFileName.IsDirWritable() )
    {
        autoSaveFileName.SetPath( wxFileName::GetTempDir() );

        if( !autoSaveFileName.IsOk() || !autoSaveFileName.IsDirWritable() )
            return false;
    }

    wxLogTrace( traceAutoSave, "Creating auto save file <" + autoSaveFileName.GetFullPath() + ">" );

    if( SavePcbFile( autoSaveFileName.GetFullPath(), false, false ) )
    {
        GetScreen()->SetModify();
        GetBoard()->SetFileName( tmpFileName.GetFullPath() );
        UpdateTitle();
        m_autoSaveState = false;

        if( !Kiface().IsSingle() &&
            GetSettingsManager()->GetCommonSettings()->m_Backup.backup_on_autosave )
        {
            GetSettingsManager()->TriggerBackupIfNeeded( NULL_REPORTER::GetInstance() );
        }

        return true;
    }

    GetBoard()->SetFileName( tmpFileName.GetFullPath() );

    return false;
}


bool PCB_EDIT_FRAME::importFile( const wxString& aFileName, int aFileType )
{
    switch( (IO_MGR::PCB_FILE_T) aFileType )
    {
    case IO_MGR::CADSTAR_PCB_ARCHIVE:
    case IO_MGR::EAGLE:
        if( OpenProjectFiles( std::vector<wxString>( 1, aFileName ), KICTL_EAGLE_BRD ) )
        {
            wxString projectpath = Kiway().Prj().GetProjectPath();
            wxFileName newfilename;

            newfilename.SetPath( Prj().GetProjectPath() );
            newfilename.SetName( Prj().GetProjectName() );
            newfilename.SetExt( KiCadPcbFileExtension );

            GetBoard()->SetFileName( newfilename.GetFullPath() );
            UpdateTitle();
            OnModify();

            // Extract a footprint library from the design and add it to the fp-lib-table
            wxString newLibPath;
            ArchiveModulesOnBoard( true, newfilename.GetName(), &newLibPath );

            if( newLibPath.Length() > 0 )
            {
                FP_LIB_TABLE* prjlibtable = Prj().PcbFootprintLibs();
                const wxString& project_env = PROJECT_VAR_NAME;
                wxString rel_path, env_path;

                wxGetEnv( project_env, &env_path );

                wxString result( newLibPath );
                rel_path =  result.Replace( env_path,
                                            wxString( "$(" + project_env + ")" ) ) ? result : "" ;

                if( !rel_path.IsEmpty() )
                    newLibPath = rel_path;

                FP_LIB_TABLE_ROW* row = new FP_LIB_TABLE_ROW( newfilename.GetName(),
                        newLibPath, wxT( "KiCad" ), wxEmptyString );
                prjlibtable->InsertRow( row );
            }

            if( !GetBoard()->GetFileName().IsEmpty() )
            {
                wxString tblName = Prj().FootprintLibTblName();

                try
                {
                    Prj().PcbFootprintLibs()->Save( tblName );
                }
                catch( const IO_ERROR& ioe )
                {
                    wxString msg = wxString::Format( _(
                                    "Error occurred saving project specific footprint library "
                                    "table:\n\n%s" ), ioe.What() );
                    wxMessageBox( msg, _( "File Save Error" ), wxOK | wxICON_ERROR );
                }
            }


            // Update module LIB_IDs to point to the just imported Eagle library
            for( MODULE* module : GetBoard()->Modules() )
            {
                LIB_ID libId = module->GetFPID();

                if( libId.GetLibItemName().empty() )
                    continue;

                libId.SetLibNickname( newfilename.GetName() );
                module->SetFPID( libId );
            }


            // Store net names for all pads, to create net remap information
            std::unordered_map<D_PAD*, wxString> netMap;

            for( const auto& pad : GetBoard()->GetPads() )
            {
                NETINFO_ITEM* netinfo = pad->GetNet();

                if( netinfo->GetNet() > 0 && !netinfo->GetNetname().IsEmpty() )
                    netMap[pad] = netinfo->GetNetname();
            }

            // Two stage netlist update:
            // - first, assign valid timestamps to footprints (no reannotation)
            // - second, perform schematic annotation and update footprint references
            //   based on timestamps
            NETLIST netlist;
            FetchNetlistFromSchematic( netlist, NO_ANNOTATION );
            DoUpdatePCBFromNetlist( netlist, false );
            FetchNetlistFromSchematic( netlist, QUIET_ANNOTATION );
            DoUpdatePCBFromNetlist( netlist, true );

            std::unordered_map<wxString, wxString> netRemap;

            // Compare the old net names with the new net names and create a net map
            for( const auto& pad : GetBoard()->GetPads() )
            {
                auto it = netMap.find( pad );

                if( it == netMap.end() )
                    continue;

                NETINFO_ITEM* netinfo = pad->GetNet();

                // Net name has changed, create a remap entry
                if( netinfo->GetNet() > 0 && netMap[pad] != netinfo->GetNetname() )
                    netRemap[netMap[pad]] = netinfo->GetNetname();
            }

            if( !netRemap.empty() )
                fixEagleNets( netRemap );

            return true;
        }

        return false;

    default:
        return false;
    }

    return false;
}


bool PCB_EDIT_FRAME::fixEagleNets( const std::unordered_map<wxString, wxString>& aRemap )
{
    bool result = true;
    BOARD* board = GetBoard();

    // perform netlist matching to prevent orphaned zones.
    for( auto zone : board->Zones() )
    {
        auto it = aRemap.find( zone->GetNet()->GetNetname() );

        if( it != aRemap.end() )
        {
            NETINFO_ITEM* net = board->FindNet( it->second );

            if( !net )
            {
                wxFAIL;
                result = false;
                continue;
            }

            zone->SetNet( net );
        }
    }


    // perform netlist matching to prevent orphaned tracks/vias.
    for( auto track : board->Tracks() )
    {
        auto it = aRemap.find( track->GetNet()->GetNetname() );

        if( it != aRemap.end() )
        {
            NETINFO_ITEM* net = board->FindNet( it->second );

            if( !net )
            {
                wxFAIL;
                result = false;
                continue;
            }

            track->SetNet( net );
        }
    }

    return result;
}
