#include "AISettingsDialog.hpp"

#include <memory>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/Button.hpp"

#include "libslic3r/AppConfig.hpp"
#include "slic3r/Utils/AIProvider.hpp"

namespace Slic3r { namespace GUI {

// Provider dropdown order <-> AppConfig provider key.
static std::string provider_key_for_index(int idx)
{
    switch (idx) {
        case 0:  return "openai";
        case 1:  return "anthropic";
        case 2:  return "compatible";
        default: return "";
    }
}

static int index_for_provider_key(const std::string &key)
{
    if (key == "anthropic")                                return 1;
    if (key == "compatible" || key == "openai_compatible") return 2;
    return 0; // default: openai
}

AISettingsDialog::AISettingsDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("AI Settings"), wxDefaultPosition,
                wxSize(46 * wxGetApp().em_unit(), -1), wxDEFAULT_DIALOG_STYLE)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);

    const int em     = wxGetApp().em_unit();
    const wxSize ctl = wxSize(FromDIP(300), -1);

    auto *grid = new wxFlexGridSizer(0, 2, FromDIP(8), FromDIP(10));
    grid->AddGrowableCol(1, 1);

    auto add_row = [&](const wxString &label, wxWindow *ctrl) {
        auto *st = new wxStaticText(this, wxID_ANY, label);
        grid->Add(st, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(ctrl, 1, wxEXPAND);
    };

    m_provider = new ComboBox(this, wxID_ANY, "", wxDefaultPosition, ctl, 0, nullptr, wxCB_READONLY);
    m_provider->Append(_L("OpenAI"));
    m_provider->Append(_L("Anthropic"));
    m_provider->Append(_L("OpenAI-compatible (custom gateway)"));
    add_row(_L("Provider"), m_provider);

    m_api_key = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, ctl, wxTE_PASSWORD);
    add_row(_L("API key"), m_api_key);

    m_gateway_url = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, ctl);
    add_row(_L("Gateway URL"), m_gateway_url);

    m_model = new ComboBox(this, wxID_ANY, "", wxDefaultPosition, ctl, 0, nullptr, 0);
    add_row(_L("Model"), m_model);

    // Test / Fetch buttons.
    m_test_btn  = new Button(this, _L("Test connection"));
    m_fetch_btn = new Button(this, _L("Fetch models"));
    auto *btn_row = new wxBoxSizer(wxHORIZONTAL);
    btn_row->Add(m_test_btn, 0, wxRIGHT, FromDIP(8));
    btn_row->Add(m_fetch_btn, 0);

    m_gateway_hint = new wxStaticText(this, wxID_ANY,
        _L("Gateway URL is used only for an OpenAI-compatible provider (Ollama, LiteLLM, a proxy...)."));
    m_status = new wxStaticText(this, wxID_ANY, "");

    // Assemble.
    auto *top = new wxBoxSizer(wxVERTICAL);
    top->Add(grid,           0, wxEXPAND | wxALL, FromDIP(16));
    top->Add(m_gateway_hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));
    top->Add(btn_row,        0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(16));
    top->Add(m_status,       0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, FromDIP(16));

    // OK / Cancel row (this fork has no DialogButtons widget).
    auto *ok_btn     = new Button(this, _L("OK"));
    auto *cancel_btn = new Button(this, _L("Cancel"));
    auto *dlg_btns   = new wxBoxSizer(wxHORIZONTAL);
    dlg_btns->AddStretchSpacer(1);
    dlg_btns->Add(ok_btn,     0, wxRIGHT, FromDIP(8));
    dlg_btns->Add(cancel_btn, 0);
    top->Add(dlg_btns, 0, wxEXPAND | wxALL, FromDIP(12));

    // Wiring.
    m_provider->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { on_provider_changed(); });
    m_test_btn->Bind(wxEVT_BUTTON,   [this](wxCommandEvent &) { on_test(); });
    m_fetch_btn->Bind(wxEVT_BUTTON,  [this](wxCommandEvent &) { on_fetch_models(); });
    ok_btn->Bind(wxEVT_BUTTON,       [this](wxCommandEvent &) { save_to_config(); EndModal(wxID_OK); });
    cancel_btn->Bind(wxEVT_BUTTON,   [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });

    load_from_config();
    on_provider_changed();

    SetSizer(top);
    top->SetSizeHints(this);
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);

    (void) em;
}

void AISettingsDialog::load_from_config()
{
    auto *ac = wxGetApp().app_config;
    if (ac == nullptr)
        return;
    m_provider->SetSelection(index_for_provider_key(ac->get("ai_slicer", "provider")));
    m_api_key->SetValue(from_u8(ac->get("ai_slicer", "api_key")));
    m_gateway_url->SetValue(from_u8(ac->get("ai_slicer", "gateway_url")));
    const std::string model = ac->get("ai_slicer", "model");
    if (! model.empty()) {
        m_model->Append(from_u8(model));
        m_model->SetValue(from_u8(model));
    }
}

void AISettingsDialog::save_to_config()
{
    auto *ac = wxGetApp().app_config;
    if (ac == nullptr)
        return;
    ac->set("ai_slicer", "provider",    provider_key_for_index(m_provider->GetSelection()));
    ac->set("ai_slicer", "api_key",     into_u8(m_api_key->GetValue()));
    ac->set("ai_slicer", "gateway_url", into_u8(m_gateway_url->GetValue()));
    ac->set("ai_slicer", "model",       into_u8(m_model->GetValue()));
    ac->save();
}

void AISettingsDialog::on_provider_changed()
{
    const bool compatible = m_provider->GetSelection() == 2;
    m_gateway_url->Enable(compatible);
    m_gateway_hint->Enable(compatible);
}

static AIConfig make_config_from_fields(ComboBox *provider, wxTextCtrl *key, wxTextCtrl *url, ComboBox *model)
{
    AIConfig cfg;
    cfg.provider = provider_key_for_index(provider->GetSelection());
    cfg.api_key  = into_u8(key->GetValue());
    cfg.base_url = into_u8(url->GetValue());
    cfg.model    = into_u8(model->GetValue());
    return cfg;
}

void AISettingsDialog::on_test()
{
    std::unique_ptr<AIProvider> provider(AIProvider::create(
        make_config_from_fields(m_provider, m_api_key, m_gateway_url, m_model)));
    if (! provider) {
        set_status(_L("Select a provider and enter an API key (and a gateway URL for the compatible provider)."), true);
        return;
    }
    set_status(_L("Testing..."));
    std::string error;
    bool ok = false;
    {
        wxBusyCursor wait; // the round-trip briefly blocks the UI, as the printer Test button does
        ok = provider->test_connection(error);
    }
    if (ok)
        set_status(_L("Connection OK."));
    else
        set_status(_L("Failed: ") + from_u8(error), true);
}

void AISettingsDialog::on_fetch_models()
{
    std::unique_ptr<AIProvider> provider(AIProvider::create(
        make_config_from_fields(m_provider, m_api_key, m_gateway_url, m_model)));
    if (! provider) {
        set_status(_L("Select a provider and enter an API key first."), true);
        return;
    }
    set_status(_L("Fetching models..."));
    std::vector<AIModelInfo> models;
    std::string error;
    bool ok = false;
    {
        wxBusyCursor wait;
        ok = provider->get_models(models, error);
    }
    if (! ok) {
        set_status(_L("Fetch failed: ") + from_u8(error), true);
        return;
    }
    const wxString keep = m_model->GetValue();
    m_model->Clear();
    for (const auto &m : models)
        m_model->Append(from_u8(m.id));
    if (! keep.IsEmpty())
        m_model->SetValue(keep);
    set_status(wxString::Format(_L("Fetched %d models."), (int) models.size()));
}

void AISettingsDialog::set_status(const wxString &msg, bool error)
{
    if (m_status == nullptr)
        return;
    m_status->SetForegroundColour(error ? wxColour(0xC0, 0x30, 0x30) : wxColour(0x30, 0x80, 0x30));
    m_status->SetLabel(msg);
    Layout();
}

void AISettingsDialog::on_dpi_changed(const wxRect & /*suggested_rect*/)
{
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
