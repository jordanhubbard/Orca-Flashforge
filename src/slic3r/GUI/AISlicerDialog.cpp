#include "AISlicerDialog.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#include <boost/filesystem.hpp>

#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_ObjectList.hpp"
#include "I18N.hpp"
#include "Plater.hpp"
#include "GLCanvas3D.hpp"
#include "Widgets/Button.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Format/bbs_3mf.hpp"

#include "slic3r/Utils/AIProvider.hpp"
#include "slic3r/Utils/AIPrinterContext.hpp"
#include "slic3r/Utils/AIShapeGen.hpp"
#include "slic3r/Utils/Http.hpp"

namespace Slic3r { namespace GUI {

// Bed extents (mm) from the active machine config, for the printable-fit check.
static void compute_bed_dims(const DynamicPrintConfig &cfg, double &x, double &y, double &z)
{
    z = (cfg.option("printable_height") != nullptr) ? cfg.opt_float("printable_height") : 250.0;
    x = y = 220.0;
    if (auto *pa = cfg.option<ConfigOptionPoints>("printable_area")) {
        double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
        for (const Vec2d &p : pa->values) {
            minx = std::min(minx, p.x()); maxx = std::max(maxx, p.x());
            miny = std::min(miny, p.y()); maxy = std::max(maxy, p.y());
        }
        if (maxx > minx && maxy > miny) { x = maxx - minx; y = maxy - miny; }
    }
}

AISlicerDialog::AISlicerDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("AI"), wxDefaultPosition,
                wxSize(52 * wxGetApp().em_unit(), -1), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);

    auto *nb = new wxNotebook(this, wxID_ANY);
    nb->AddPage(build_generate_tab(nb), _L("Generate shape"), true);
    nb->AddPage(build_search_tab(nb),   _L("Search & import"), false);

    auto *close_btn = new Button(this, _L("Close"));
    close_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CLOSE); });
    auto *btn_row = new wxBoxSizer(wxHORIZONTAL);
    btn_row->AddStretchSpacer(1);
    btn_row->Add(close_btn, 0);

    auto *top = new wxBoxSizer(wxVERTICAL);
    top->Add(nb,      1, wxEXPAND | wxALL, FromDIP(12));
    top->Add(btn_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    SetSizer(top);
    top->SetSizeHints(this);
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

wxWindow *AISlicerDialog::build_generate_tab(wxNotebook *nb)
{
    auto *panel = new wxPanel(nb, wxID_ANY);
    auto *s = new wxBoxSizer(wxVERTICAL);

    s->Add(new wxStaticText(panel, wxID_ANY, _L("Describe the object you want to create:")),
           0, wxALL, FromDIP(10));

    m_prompt = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition,
                              wxSize(FromDIP(420), FromDIP(110)), wxTE_MULTILINE);
    s->Add(m_prompt, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    s->Add(new wxStaticText(panel, wxID_ANY,
               _L("Your printer's settings and live sensor data (bed, nozzle, material, temperatures, "
                  "and a camera frame when available) are sent with each request, so the shape fits the machine.")),
           0, wxALL, FromDIP(10));

    m_generate_btn = new Button(panel, _L("Generate"));
    m_generate_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_generate(); });
    s->Add(m_generate_btn, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    m_status = new wxStaticText(panel, wxID_ANY, "");
    s->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    panel->SetSizer(s);
    return panel;
}

void AISlicerDialog::on_generate()
{
    const wxString promptw = m_prompt->GetValue();
    if (promptw.IsEmpty()) { set_status(_L("Enter a description first."), true); return; }

    auto *ac = wxGetApp().app_config;
    std::unique_ptr<AIProvider> provider(
        ac ? AIProvider::create(AIProvider::config_from_app_config(*ac)) : nullptr);
    if (! provider) {
        set_status(_L("No AI provider is configured. Open AI Settings and register a provider and API key first."), true);
        return;
    }

    // Assemble the request: shape-spec contract + machine context as system, the
    // user's description (and a camera frame, if any) as the user turn.
    AIPrinterContext ctx = AIPrinterContext::gather();

    std::vector<AIMessage> messages;
    AIMessage sys;
    sys.role    = "system";
    sys.content = ai_shape_spec_instructions() +
                  "\n\nPrinter context (respect these constraints; never exceed the build volume):\n" +
                  ctx.to_prompt();
    messages.push_back(std::move(sys));

    AIMessage user;
    user.role    = "user";
    user.content = into_u8(promptw);
    if (ctx.has_image())
        user.images.push_back(ctx.to_image());
    messages.push_back(std::move(user));

    set_status(_L("Generating..."));
    AIResponse resp;
    {
        wxBusyCursor wait; // the LLM round-trip briefly blocks the UI
        resp = provider->chat(messages);
    }
    if (! resp.ok) { set_status(_L("AI request failed: ") + from_u8(resp.error), true); return; }

    Model result;
    std::string error;
    const std::string name = into_u8(promptw).substr(0, 40);
    if (! ai_build_model_from_response(resp.content, name, result, error)) {
        set_status(_L("Could not build a model from the AI response: ") + from_u8(error), true);
        return;
    }
    if (result.objects.empty()) { set_status(_L("The AI did not return a usable shape."), true); return; }

    // Printable-fit check (informational — we still add it so the user can rescale).
    double bx = 220, by = 220, bz = 250;
    if (auto *bundle = wxGetApp().preset_bundle)
        compute_bed_dims(bundle->full_config(), bx, by, bz);
    std::string warn;
    const bool fits = ai_model_fits_bed(result, bx, by, bz, warn);

    // Inject on the main thread (we are on it): mutate the Plater model, then
    // register + select via the object list.
    Plater *plater = wxGetApp().plater();
    plater->take_snapshot(into_u8(_L("Add AI generated object")));
    Model &model = plater->model();
    std::vector<size_t> idxs;
    for (const ModelObject *src : result.objects) {
        ModelObject *dst = model.add_object(*src);
        dst->ensure_on_bed();
        idxs.push_back(model.objects.size() - 1);
    }
    wxGetApp().obj_list()->paste_objects_into_list(idxs);
    if (auto *canvas = plater->get_view3D_canvas3D())
        canvas->reload_scene(true);

    if (fits)
        set_status(_L("Added the generated object to the plate."));
    else
        set_status(from_u8(warn) + " " + _L("Added anyway — scale it to fit."), true);
}

void AISlicerDialog::set_status(const wxString &msg, bool error)
{
    if (m_status == nullptr)
        return;
    m_status->SetForegroundColour(error ? wxColour(0xC0, 0x30, 0x30) : wxColour(0x30, 0x80, 0x30));
    m_status->SetLabel(msg);
    Layout();
}

// --- Search & Import -------------------------------------------------------

// True if the URL's path ends in a model extension we can load.
static bool is_supported_model_url(const std::string &url)
{
    std::string path = url;
    auto q = path.find_first_of("?#");
    if (q != std::string::npos) path = path.substr(0, q);
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char) std::tolower(c); });
    for (const char *ext : {".stl", ".3mf", ".obj", ".step", ".stp"})
        if (lower.size() >= std::strlen(ext) && lower.compare(lower.size() - std::strlen(ext), std::strlen(ext), ext) == 0)
            return true;
    return false;
}

static std::string filename_from_url(const std::string &url)
{
    std::string path = url;
    auto q = path.find_first_of("?#");
    if (q != std::string::npos) path = path.substr(0, q);
    auto slash = path.find_last_of('/');
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (name.empty()) name = "ai_model.stl";
    return name;
}

wxWindow *AISlicerDialog::build_search_tab(wxNotebook *nb)
{
    auto *panel = new wxPanel(nb, wxID_ANY);
    auto *s = new wxBoxSizer(wxVERTICAL);

    s->Add(new wxStaticText(panel, wxID_ANY, _L("Search the web for a model to import:")),
           0, wxALL, FromDIP(10));

    auto *query_row = new wxBoxSizer(wxHORIZONTAL);
    m_query = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(FromDIP(320), -1), wxTE_PROCESS_ENTER);
    m_search_btn = new Button(panel, _L("Search"));
    m_search_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_search(); });
    m_query->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { on_search(); });
    query_row->Add(m_query, 1, wxRIGHT, FromDIP(8));
    query_row->Add(m_search_btn, 0);
    s->Add(query_row, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    m_results_list = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(420), FromDIP(140)));
    s->Add(m_results_list, 1, wxEXPAND | wxALL, FromDIP(10));

    m_import_btn = new Button(panel, _L("Import selected"));
    m_import_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_import(); });
    s->Add(m_import_btn, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    m_search_status = new wxStaticText(panel, wxID_ANY, "");
    s->Add(m_search_status, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    panel->SetSizer(s);
    return panel;
}

void AISlicerDialog::on_search()
{
    const wxString queryw = m_query->GetValue();
    if (queryw.IsEmpty()) { set_search_status(_L("Enter a search query first."), true); return; }

    auto *ac = wxGetApp().app_config;
    std::unique_ptr<AIProvider> provider(
        ac ? AIProvider::create(AIProvider::config_from_app_config(*ac)) : nullptr);
    if (! provider) {
        set_search_status(_L("No AI provider is configured. Open AI Settings first."), true);
        return;
    }

    std::vector<AIMessage> messages;
    AIMessage sys;
    sys.role    = "system";
    sys.content =
        "You are a search assistant for 3D-printable models. Given the user's request, return ONLY a "
        "JSON array (no prose) of up to 8 directly-downloadable 3D model files. Each item is "
        "{\"title\":string,\"url\":string,\"source\":string}. Each url MUST be a direct, publicly "
        "downloadable link ending in .stl, .3mf, .obj, .step or .stp, and must respect the host's terms "
        "of use. If you are unsure a link is valid, omit it. Return [] if you cannot find any.";
    messages.push_back(std::move(sys));
    AIMessage user; user.role = "user"; user.content = into_u8(queryw);
    messages.push_back(std::move(user));

    set_search_status(_L("Searching..."));
    AIResponse resp;
    {
        wxBusyCursor wait;
        resp = provider->chat(messages);
    }
    if (! resp.ok) { set_search_status(_L("Search failed: ") + from_u8(resp.error), true); return; }

    // Extract the JSON array from the reply.
    std::string body = resp.content;
    auto lb = body.find('['), rb = body.rfind(']');
    if (lb == std::string::npos || rb == std::string::npos || rb < lb) {
        set_search_status(_L("The AI did not return any results."), true);
        return;
    }
    m_results_list->Clear();
    m_result_urls.clear();
    try {
        nlohmann::json arr = nlohmann::json::parse(body.substr(lb, rb - lb + 1));
        for (const auto &item : arr) {
            if (! item.is_object()) continue;
            const std::string url = item.value("url", std::string());
            if (url.empty() || ! is_supported_model_url(url)) continue;
            const std::string title  = item.value("title",  std::string("(untitled)"));
            const std::string source = item.value("source", std::string());
            m_result_urls.push_back(url);
            wxString label = from_u8(title);
            if (! source.empty()) label += " — " + from_u8(source);
            m_results_list->Append(label);
        }
    } catch (const std::exception &e) {
        set_search_status(_L("Could not parse the search results: ") + from_u8(e.what()), true);
        return;
    }

    if (m_result_urls.empty())
        set_search_status(_L("No directly-downloadable models were found. Try a different query."), true);
    else
        set_search_status(wxString::Format(_L("Found %d model(s). Select one and Import."), (int) m_result_urls.size()));
}

void AISlicerDialog::on_import()
{
    const int sel = m_results_list->GetSelection();
    if (sel == wxNOT_FOUND || sel < 0 || sel >= (int) m_result_urls.size()) {
        set_search_status(_L("Select a model from the list first."), true);
        return;
    }
    const std::string url = m_result_urls[sel];

    std::string dir = wxGetApp().app_config ? wxGetApp().app_config->get("download_path") : std::string();
    if (dir.empty())
        dir = data_dir();

    boost::filesystem::path out;
    std::string body;
    bool ok = false;
    std::string err;
    set_search_status(_L("Downloading..."));
    {
        wxBusyCursor wait;
        Http::get(url)
            .size_limit(static_cast<size_t>(256) * 1024 * 1024)
            .on_complete([&](std::string b, unsigned status) { if (status >= 200 && status < 300) { body = std::move(b); ok = true; } })
            .on_error([&](std::string, std::string error, unsigned status) {
                err = error.empty() ? ("HTTP " + std::to_string(status)) : error;
            })
            .perform_sync();
    }
    if (! ok || body.empty()) { set_search_status(_L("Download failed: ") + from_u8(err), true); return; }

    try {
        out = boost::filesystem::path(dir) / filename_from_url(url);
        std::ofstream f(out.string(), std::ios::binary);
        if (! f) { set_search_status(_L("Could not write the downloaded file."), true); return; }
        f.write(body.data(), static_cast<std::streamsize>(body.size()));
    } catch (const std::exception &e) {
        set_search_status(_L("Could not save the model: ") + from_u8(e.what()), true);
        return;
    }

    auto idxs = wxGetApp().plater()->load_files(std::vector<std::string>{ out.string() },
                                                LoadStrategy::LoadModel, false);
    if (idxs.empty())
        set_search_status(_L("Downloaded, but the model could not be loaded."), true);
    else
        set_search_status(_L("Imported the model onto the plate."));
}

void AISlicerDialog::set_search_status(const wxString &msg, bool error)
{
    if (m_search_status == nullptr)
        return;
    m_search_status->SetForegroundColour(error ? wxColour(0xC0, 0x30, 0x30) : wxColour(0x30, 0x80, 0x30));
    m_search_status->SetLabel(msg);
    Layout();
}

void AISlicerDialog::on_dpi_changed(const wxRect & /*suggested_rect*/)
{
    Fit();
    Refresh();
}

}} // namespace Slic3r::GUI
