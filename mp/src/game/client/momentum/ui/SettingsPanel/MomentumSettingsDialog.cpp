#include "cbase.h"

#include "IMomentumSettingsPanel.h"
#include "SettingsPage.h"
#include "HudSettingsPage.h"
#include "ControlsSettingsPage.h"
#include "ComparisonsSettingsPage.h"
#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/pch_vgui_controls.h>

#include "tier0/memdbgon.h"

using namespace vgui;

class CMomentumSettingsPanel : public PropertyDialog
{
    DECLARE_CLASS_SIMPLE(CMomentumSettingsPanel, PropertyDialog);
    // CMomentumSettingsPanel : This Class / vgui::Frame : BaseClass

    CMomentumSettingsPanel(VPANEL parent); // Constructor
    ~CMomentumSettingsPanel();           // Destructor

  protected:
    // VGUI overrides:
    void OnTick() override;
    void OnClose() override
    {
        BaseClass::OnClose();
        PostActionSignal(new KeyValues("OnClose"));
    }

  private:
    SettingsPage *m_pHudSettings, *m_pControlsSettings, *m_pCompareSettings;
};

// Constuctor: Initializes the Panel
CMomentumSettingsPanel::CMomentumSettingsPanel(VPANEL parent) : BaseClass(nullptr, "CMomentumSettingsPanel")
{
    SetParent(parent);
    SetAutoDelete(true);
    LoadControlSettings("resource/ui/SettingsPanel_Base.res");
    SetKeyBoardInputEnabled(true);
    SetMouseInputEnabled(true);

    SetMinimumSize(500, 500);
    SetApplyButtonVisible(true);
    SetTitleBarVisible(true);
    SetMinimizeButtonVisible(false);
    SetMaximizeButtonVisible(false);
    SetCloseButtonVisible(true);
    SetMoveable(true);
    SetVisible(false);

    //Create the pages here
    m_pControlsSettings = new ControlsSettingsPage(this);
    m_pHudSettings = new HudSettingsPage(this);
    m_pCompareSettings = new ComparisonsSettingsPage(this);

    //Note: we're adding the scroll panels here, because we want to be able to scroll.
    AddPage(m_pControlsSettings->GetScrollPanel(), "#MOM_Settings_Tab_Controls");
    AddPage(m_pHudSettings->GetScrollPanel(), "#MOM_Settings_Tab_HUD");
    AddPage(m_pCompareSettings->GetScrollPanel(), "#MOM_Settings_Tab_Comparisons");
    //MOM_TODO: Add the other settings panels here.

    SetScheme("SourceScheme");

    ivgui()->AddTickSignal(GetVPanel());
}

CMomentumSettingsPanel::~CMomentumSettingsPanel()
{
}

// Class: CMyPanelInterface Class. Used for construction.
class CMomentumSettingsPanelInterface : public IMomentumSettingsPanel
{
  private:
    CMomentumSettingsPanel *settings_panel;

  public:
    CMomentumSettingsPanelInterface() { settings_panel = nullptr; }
    ~CMomentumSettingsPanelInterface() { settings_panel = nullptr; }
    void Create(VPANEL parent) override { settings_panel = new CMomentumSettingsPanel(parent); }
    void Destroy() override
    {
        if (settings_panel)
        {
            settings_panel->SetParent(nullptr);
            settings_panel->DeletePanel();
        }
        settings_panel = nullptr;
    }
    void Activate(void) override
    {
        if (settings_panel)
        {
            settings_panel->Activate();
        }
    }
    void Close() override
    {
        if (settings_panel)
        {
            settings_panel->Close();
        }
    }
};

//Expose this interface to the DLL
static CMomentumSettingsPanelInterface g_SettingsPanel;
IMomentumSettingsPanel *momentum_settings = static_cast<IMomentumSettingsPanel *>(&g_SettingsPanel);

CON_COMMAND_F(mom_settings_show, "Shows the settings panel.\n",
              FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_HIDDEN)
{
    momentum_settings->Activate();
}

void CMomentumSettingsPanel::OnTick()
{
    if (g_pClientMode->GetViewport() && g_pClientMode->GetViewportAnimationController())
        g_pClientMode->GetViewportAnimationController()->UpdateAnimations(system()->GetFrameTime());
}