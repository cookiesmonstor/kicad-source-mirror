///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.9.0 Jul 27 2020)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include "dialog_shim.h"
#include <wx/string.h>
#include <wx/radiobox.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/statline.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/statbmp.h>
#include <wx/button.h>
#include <wx/dialog.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class DIALOG_UNUSED_PAD_LAYERS_BASE
///////////////////////////////////////////////////////////////////////////////
class DIALOG_UNUSED_PAD_LAYERS_BASE : public DIALOG_SHIM
{
	private:

	protected:
		wxBoxSizer* m_MainSizer;
		wxRadioBox* m_rbScope;
		wxRadioBox* m_rbAction;
		wxCheckBox* m_cbSelectedOnly;
		wxCheckBox* m_cbPreservePads;
		wxStaticLine* m_staticline1;
		wxStaticBitmap* m_image;
		wxStdDialogButtonSizer* m_StdButtons;
		wxButton* m_StdButtonsOK;
		wxButton* m_StdButtonsCancel;

		// Virtual event handlers, overide them in your derived class
		virtual void onScopeChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void syncImages( wxCommandEvent& event ) { event.Skip(); }


	public:

		DIALOG_UNUSED_PAD_LAYERS_BASE( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Unused Pad Layers"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 602,321 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER );
		~DIALOG_UNUSED_PAD_LAYERS_BASE();

};

